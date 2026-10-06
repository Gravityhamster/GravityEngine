#pragma once
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <iostream>
#include <chrono>
#include <math.h>
#include <future>
#include <thread>
#include <vector>
#include <queue>
#include <string>
#include <unordered_map>
#include <random>
#include <map>
#include <climits>
#include "SPSC_Queue.h"
#include "GravityEngineTypes.h"

#define PI 3.14159265358979323846f

// Gravity Engine sound class
class GravityEngine_Sound
{
private:
    // Copilot help on this one
    // Convert the audio in the audio buffer to a different audio spec
    // Uint8* audio_buf : The buffer for the audio data
    // Uint32 audio_len : The length of the audio in the audio buffer
    // SDL_AudioSpec wav_audio_spec : Audio specifications of the audio to be converted
    // SDL_AudioSpec audio_spec : Audio specifications to convert the audio to
    std::vector<Uint8> ConvertAudio(Uint8* audio_buf, Uint32 audio_len, SDL_AudioSpec wav_audio_spec, SDL_AudioSpec audio_spec)
    {
        // Convert audio to spec
        auto sdl_audio_stream_conv = SDL_CreateAudioStream(&wav_audio_spec, &audio_spec);
        SDL_PutAudioStreamData(sdl_audio_stream_conv, audio_buf, audio_len);
        SDL_FlushAudioStream(sdl_audio_stream_conv);
        std::vector<Uint8> converted_data;
        Uint8 temp[4096];
        int bytesRead;
        while ((bytesRead = SDL_GetAudioStreamData(sdl_audio_stream_conv, temp, sizeof(temp))) > 0) {
            converted_data.insert(converted_data.end(), temp, temp + bytesRead);
        }
        SDL_DestroyAudioStream(sdl_audio_stream_conv);
        return converted_data;
    }

public:
    // -= Attributes =-
    std::vector<Uint8> converted_audio; // Buffer for the final converted audio data

    // -= Methods =-

    // Construct audio
    // const char* path : File path of the audio
    // SDL_AudioSpec audio_spec : Audio specification to convert the audio to (this should be the global audio spec in the engine)
    GravityEngine_Sound(const char* path, SDL_AudioSpec audio_spec)
    {
        Uint8* audio_buf;
        Uint32 audio_len;
        SDL_AudioSpec wav_audio_spec;
        // Load the wav file
        SDL_LoadWAV(path, &wav_audio_spec, &audio_buf, &audio_len);
        // Convert the audio
        converted_audio = ConvertAudio(audio_buf, audio_len, wav_audio_spec, audio_spec);
    };

    // Destruct audio
    ~GravityEngine_Sound() {};
};

// TODO: Any effects related directly to instrument automation should be implemented directly into the sampler (i.e. vibrato, pitchsweep, fadein, fadeout, etc.)

// Struct for submitting changes to the sampler parameters from the main thread to the audio thread
struct live_change_sample
{
    double freq = -9999.0;
    double pan = -2.0;
    double vol = -1.0;
    double pw = -1.0;
};

// Chamerblain filter processing - COPILOT function implemented into a sequestored function
// float sample : Current decimal audio position
// float cutoff : 0 to 1 freq filter cutoff 
// float resonance : 0 to 1 resonance frequency
// float sample_rate_freq : Audio sample rate (e.g. 48000hz)
// float* lp : Pointer to the lowpass filter state variable
// float* bp : Pointer to the bandpass filter state variable
// float* hp : Pointer to the highpass filter state variable
inline void SamplerProcessChamberlainFilter(float sample, float cutoff, float resonance, float sample_rate_freq, float* lp, float* bp, float* hp)
{
    // Apply filter
    float warped = cutoff * cutoff * cutoff;
    float cutoff_hz = warped * (sample_rate_freq * 0.5f);
    float f = std::clamp(2.0f * sinf(PI * cutoff_hz / sample_rate_freq), 0.f, 0.999f);
    float q = std::clamp(1.0f - resonance, 0.05f, 1.f);

    // Calculate filter
    (*hp) = sample - (*lp) - q * (*bp);
    (*bp) = (*bp) + f * (*hp);
    (*lp) = (*lp) + f * (*bp);

    // Dampen output to prevent feedback looping
    (*hp) *= 0.999f;
    (*bp) *= 0.999f;
    (*lp) *= 0.999f;
}

// Voice structure
class GravityEngine_SamplerVoice
{

public:
    SpscQueue<live_change_sample, 1024> live_changes;
    std::atomic<int> frame_counter = 0;
    std::atomic<float> frames_per_tick = 0;

    // Actual audio values that are used in the audio thread
    std::atomic<float> freq = 50.0;
    std::atomic<float> volume = 1;
    std::atomic<float> panning = 0.5;

    // Staging values that are only used in tracking position in incremental changes to the sampler parameters. 
    // These are used to all movement to be smoother. So basically, the tracker thread will make a change to the
    // audio values, then write the change to the queue and then write the change to these staging values so that
    // it can continue from the last queue write value, even if the sampler has not yet applied its changes.
    std::atomic<float> stg_step_freq = 50.0;
    std::atomic<float> stg_swpd_freq = 0.0;
    std::atomic<float> stg_volume = 1;
    std::atomic<float> stg_panning = 0.5;

    std::atomic<float> pitch_freq = 0;
    std::atomic<float> volume_freq = 0;
    std::atomic<float> pan_freq = 0.0;

    // float vibrato_freq = 0; -- Not yet implemented
    // float vibrato_amp = 0; -- Not yet implemented

    // Filter
    float cutoff = 0.5f; // 0.0 - 1.0 -- TODO: Determine usable range
    float resonance = 0.5f; // 0.0 - 1.0 -- TODO: Determine usable range

    // Filter state
    float lp_l = 0.0f;
    float bp_l = 0.0f;
    float hp_l = 0.0f;
    float lp_r = 0.0f;
    float bp_r = 0.0f;
    float hp_r = 0.0f;

    FilterType filter = FilterType::none;
    FilterAlgorithm algorithm = FilterAlgorithm::chamberlain;

    // Other state variables
    float pan_phase = 0.f;

    std::atomic<bool> will_start_playing = false;
    std::atomic<bool> start_playing = false;

    long start_time_ms = 0;
    long mid_time_ms = 0;
    long end_time_ms = -1;

    float last_freq = 0;

	GravityEngine_Sound* sound = nullptr;
    bool looping = false;

    // Frequency of the original sampled audio (0 = don't pitch shift, play at original speed)
    std::atomic<float> base_freq = 0.f;

    // Replaces `long read_ptr`: playback position in FRAMES, fractional
    double read_pos = 0.0;
    // Playback rate at the end of the previous buffer (used for smooth ramping)
    double cur_rate = 1.0;
};

// Template for sampler objects
class GravityEngine_Sampler
{

    // sampler parameters
public:

    std::vector<GravityEngine_SamplerVoice*> voices;
    int sample_frames;

    // time : Milliseconds to convert to bytes
    // audio_spec : Channel audio spec
    static size_t milliseconds_to_bytes(long time, SDL_AudioSpec audio_spec)
    {
        // Bytes in every sample
        double bytes_per_sample = SDL_AUDIO_BITSIZE(audio_spec.format) / 8.0 * audio_spec.channels;
        // Samples in ever second
        double samples_per_second = audio_spec.freq;
        // Bytes per second
        double bytes_per_second = bytes_per_sample * samples_per_second;
        // Bytes per millisecond
        return (size_t)floor(time * (bytes_per_second / 1000.0));
    }

    // Claude - Milliseconds to (fractional) frames
    static double milliseconds_to_frames(long time, const SDL_AudioSpec& audio_spec)
    {
        return time * (audio_spec.freq / 1000.0);
    }

    // Claude - Playback speed needed to turn a sample recorded at base_freq into freq
    static double PlaybackRate(float freq, float base_freq)
    {
        if (base_freq <= 0.f)
            return 1.0; // no root pitch set, play as recorded
        return std::clamp((double)freq / (double)base_freq, 0.0, 16.0);
    }

    // Mostly Claude reworked
	// Reads a sample from the sound at a fractional frame position, using linear interpolation.
    // GravityEngine_sampler* sampler : sampler object reference
    // SDL_AudioStream* stream : Audio stream that the sampler audio plays on
    // SDL_AudioSpec* spec : Audio spec to format the audio with
    // SDL_AudioDeviceID dev : Device the audio will play on
    // ChannelStates* state : Current state of the channel
    // bool* sampler_playing : Flag to indicate the thread has successfully finished
    static void GenerateAudio(GravityEngine_Sampler* sampler, SDL_AudioStream* stream, SDL_AudioSpec* spec, SDL_AudioDeviceID dev, std::atomic<ChannelStates>* state, std::atomic<bool>* sampler_playing)
    {
        bool first = true;

        // Get sample frames
        SDL_GetAudioDeviceFormat(dev, spec, &sampler->sample_frames);
        if (spec->channels > 2)
            spec->channels = 2;

        const int channels = spec->channels;
        const int buffer_frames = sampler->sample_frames;
        const int bytes_per_sample = SDL_AUDIO_BITSIZE(spec->format) / 8;
        const int buffer_size = buffer_frames * channels * bytes_per_sample;
        const size_t frame_bytes = sizeof(float) * channels;
        std::vector<Uint8> buffer(buffer_size);

        SDL_SetAudioStreamGain(stream, 1.0f);

        while ((*state) == playing || (*state) == paused) {

            if ((*state) == paused)
            {
                std::this_thread::yield();
                continue;
            }

            int threshold_frames = sampler->sample_frames;
            int available_frames = SDL_GetAudioStreamAvailable(stream) / (sizeof(float) * channels);

            if (available_frames < threshold_frames)
            {
                SDL_memset(buffer.data(), 0, buffer_size);

                for (auto v : sampler->voices)
                {
                    // Voice was just triggered: initialise its state
                    if (v->will_start_playing)
                    {
                        v->freq = v->stg_step_freq.load();
                        v->volume = v->stg_volume.load();
                        v->panning = v->stg_panning.load();
                        v->stg_swpd_freq = v->stg_step_freq.load();

                        v->panning = std::clamp<float>(v->panning, 0.f, 1.f);

                        v->hp_l = v->bp_l = v->lp_l = 0.0f;
                        v->hp_r = v->bp_r = v->lp_r = 0.0f;

                        v->read_pos = v->start_time_ms != -1
                            ? milliseconds_to_frames(v->start_time_ms, *spec)
                            : 0.0;

                        // Start at the right speed so there's no pitch glide into the note
                        v->cur_rate = PlaybackRate(v->freq.load(), v->base_freq.load());

                        v->frame_counter = 0;

                        v->will_start_playing = false;
                        v->start_playing = true;
                    }

                    if (!v->start_playing || !v->sound)
                        continue;

                    // -= Apply queued parameter changes =-
                    const int fpt = std::max(1, (int)v->frames_per_tick.load());
                    if (v->frames_per_tick.load() > 0)
                    {
                        v->frame_counter += sampler->sample_frames;
                        while (v->frame_counter >= fpt)
                        {
                            v->frame_counter -= fpt;

                            live_change_sample change;
                            if (!v->live_changes.pop(change))
                                break;

                            if (change.freq != -9999) v->freq = change.freq;
                            if (change.pan != -2)     v->panning = change.pan;
                            if (change.vol != -1)     v->volume = change.vol;
                        }
                    }

                    // -= Volume and panning gains for this buffer =-
                    const float vol = v->volume.load();
                    const float pan = std::clamp<float>(v->panning.load(), 0.f, 1.f);

                    float gains[2];
                    if (channels == 2)
                    {
                        const float angle = pan * (PI / 2.0f);
                        gains[0] = std::cos(angle) * vol;
                        gains[1] = std::sin(angle) * vol;
                    }
                    else
                    {
                        gains[0] = (1.0f - std::abs(0.5f - pan)) * vol;
                        gains[1] = gains[0];
                    }

                    // -= Region of the sample that can be played (in frames) =-
                    const auto& audio = v->sound->converted_audio;
                    const float* src = (const float*)audio.data();
                    const int64_t total_frames = (int64_t)(audio.size() / frame_bytes);

                    int64_t end_frame = total_frames;
                    if (v->end_time_ms != -1)
                        end_frame = std::min<int64_t>(total_frames, (int64_t)milliseconds_to_frames(v->end_time_ms, *spec));

                    const int64_t loop_start = (int64_t)milliseconds_to_frames(v->mid_time_ms, *spec);
                    // Only loop if the loop point is before the end
                    const bool can_loop = v->looping && loop_start < end_frame;

                    // -= Playback speed, ramped across the buffer to avoid pitch stepping =-
                    const double rate_start = v->cur_rate;
                    const double rate_end = PlaybackRate(v->freq.load(), v->base_freq.load());

                    double pos = v->read_pos;

                    // Filter settings and state for this buffer. State is copied into locals for
                    // the loop and written back afterwards, so it carries over between buffers.
                    const float cutoff = v->cutoff;
                    const float resonance = v->resonance;
                    const FilterType filter_type = v->filter;
                    const bool use_filter = (filter_type != FilterType::none) &&
                        (v->algorithm == FilterAlgorithm::chamberlain);

                    float lp[2] = { v->lp_l, v->lp_r };
                    float bp[2] = { v->bp_l, v->bp_r };
                    float hp[2] = { v->hp_l, v->hp_r };

                    for (int i = 0; i < buffer_frames; i++)
                    {
                        // Past the end of the playable region?
                        if (pos >= (double)end_frame)
                        {
                            if (can_loop)
                            {
                                // fmod keeps the fractional part, so looping doesn't drift in pitch
                                pos = loop_start + std::fmod(pos - loop_start, (double)(end_frame - loop_start));
                            }
                            else
                            {
                                v->start_playing = false;
                                pos = 0.0;
                                break;
                            }
                        }

                        // Linear interpolation between the two neighbouring frames
                        const int64_t i0 = (int64_t)pos;
                        int64_t i1 = i0 + 1;
                        if (i1 >= end_frame)
                            i1 = can_loop ? loop_start : i0;
                        const float frac = (float)(pos - (double)i0);

                        for (int c = 0; c < channels; c++)
                        {
                            const float a = src[i0 * channels + c];
                            const float b = src[i1 * channels + c];

                            // Pitched sample with volume and pan applied (same as the synth's left_sample / right_sample)
                            float s = (a + (b - a) * frac) * gains[c];

                            // Filter
                            if (use_filter)
                            {
                                SamplerProcessChamberlainFilter(s, cutoff, resonance, (float)spec->freq, &lp[c], &bp[c], &hp[c]);

                                if (filter_type == FilterType::lowpass)       s = lp[c];
                                else if (filter_type == FilterType::highpass) s = hp[c];
                                else if (filter_type == FilterType::bandpass) s = bp[c];
                            }

                            ((float*)buffer.data())[i * channels + c] += s;
                        }

                        // Advance by the current rate
                        const double rate = rate_start + (rate_end - rate_start) * ((double)i / buffer_frames);
                        pos += rate;
                    }

                    // Save the filter state for the next buffer
                    v->lp_l = lp[0]; v->bp_l = bp[0]; v->hp_l = hp[0];
                    v->lp_r = lp[1]; v->bp_r = bp[1]; v->hp_r = hp[1];

                    v->read_pos = pos;
                    v->cur_rate = rate_end;
                }

                // Clamp so overlapping voices can't exceed full scale
                float* mix = (float*)buffer.data();
                const int total_samples = buffer_size / (int)sizeof(float);
                for (int i = 0; i < total_samples; i++)
                    mix[i] = std::clamp(mix[i], -1.0f, 1.0f);

                SDL_PutAudioStreamData(stream, buffer.data(), buffer_size);
            }

            if (first == true)
            {
                SDL_BindAudioStream(dev, stream);
                SDL_ResumeAudioDevice(dev);
                first = false;
            }
        }
        (*sampler_playing) = false;
    }

    // Automate the sampler modulation variables (Effected by call rate)
    void SampleAutomation(double* new_freq, double* new_pan, double* new_vol, int voice_index)
    {
        // Step panning
        if (voices[voice_index]->pan_freq > 0)
        {
            voices[voice_index]->pan_phase += voices[voice_index]->pan_freq / 100;
            (*new_pan) = (sin(voices[voice_index]->pan_phase * 2. * PI) / 2) + 0.5;
            if (voices[voice_index]->pan_phase > 1.)
                voices[voice_index]->pan_phase -= 1.;
            voices[voice_index]->stg_panning = (*new_pan);
        }
        // Step note
        if (voices[voice_index]->pitch_freq != 0)
        {
            if (voices[voice_index]->pitch_freq > 0)
                voices[voice_index]->stg_swpd_freq = voices[voice_index]->stg_swpd_freq * (voices[voice_index]->pitch_freq + 1); // Stage the new freq change
            if (voices[voice_index]->pitch_freq < 0)
                voices[voice_index]->stg_swpd_freq = voices[voice_index]->stg_swpd_freq / (abs(voices[voice_index]->pitch_freq) + 1); // Stage the new freq change
            auto freq_swp_ofst = voices[voice_index]->stg_swpd_freq - voices[voice_index]->stg_step_freq;
            (*new_freq) += freq_swp_ofst;
        }
        // Step volumne
        if (voices[voice_index]->volume_freq != 0)
        {
            (*new_vol) = voices[voice_index]->stg_volume + voices[voice_index]->volume_freq / 100;
            if ((*new_vol) < 0)
                (*new_vol) = 0;
            voices[voice_index]->stg_volume = (*new_vol);
        }
    }
};
