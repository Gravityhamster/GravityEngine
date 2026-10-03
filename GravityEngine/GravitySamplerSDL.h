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
#include "SPSC_Queue.h"
#include "GravityEngineTypes.h"

#define PI 3.14159265358979323846f

// TODO: Any effects related directly to instrument automation should be implemented directly into the sampler (i.e. vibrato, pitchsweep, fadein, fadeout, etc.)
// TODO: Acquire personal understanding of COPILOT marked code and rewrite it myself

// Struct for submitting changes to the sampler parameters from the main thread to the audio thread
struct live_change_sample
{
    double freq = -9999.0;
    double pan = -2.0;
    double vol = -1.0;
    double pw = -1.0;
};

// Enum to define the type of filter applied to audio channel
enum class SampleFilterType
{
    lowpass,
    highpass,
    bandpass,
    none,
    min = lowpass,
    max = none
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

// Enum to define the wave forms on a sampler
enum class SampleWaveForm
{
    sample_sine,
    sample_square,
    sample_pulse,
    sample_sawtooth,
    sample_triangle,
    sample_noise,
    min = sample_sine,
    max = sample_noise
};

// Enum to define filter algorithm
enum class SampleFilterAlgorithm
{
    chamberlain
};

// Conversion map for waveforms
inline std::map<SampleWaveForm, std::string> sample_waveform_to_string = {
    {SampleWaveForm::sample_sine, "SINE"},
    {SampleWaveForm::sample_square, "SQUARE"},
    {SampleWaveForm::sample_pulse, "PULSE"},
    {SampleWaveForm::sample_sawtooth, "SAWTOOTH"},
    {SampleWaveForm::sample_triangle, "TRIANGLE"},
    {SampleWaveForm::sample_noise, "NOISE"}
};

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
    std::atomic<float> pulse_width = 0.5;

    // Staging values that are only used in tracking position in incremental changes to the sampler parameters. 
    // These are used to all movement to be smoother. So basically, the tracker thread will make a change to the
    // audio values, then write the change to the queue and then write the change to these staging values so that
    // it can continue from the last queue write value, even if the sampler has not yet applied its changes.
    std::atomic<float> stg_base_freq = 50.0;
    std::atomic<float> stg_swpd_freq = 0.0;
    std::atomic<float> stg_volume = 1;
    std::atomic<float> stg_panning = 0.5;
    std::atomic<float> stg_pulse_width = 0.5;

    std::atomic<float> pitch_freq = 0;
    std::atomic<float> volume_freq = 0;
    std::atomic<float> pan_freq = 0.0;
    std::atomic<float> pulse_width_freq = 0.0;

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

    SampleWaveForm waveform = SampleWaveForm::sample_sine;
    SampleFilterType filter = SampleFilterType::none;
    SampleFilterAlgorithm algorithm = SampleFilterAlgorithm::chamberlain;

    // Other state variables
    float pan_phase = 0.f;
    float pw_phase = 0.f;

    std::atomic<bool> will_start_playing = false;
    std::atomic<bool> start_playing = false;

    float phase = 0.;
    float last_freq = 0;
};

// Template for sampler objects
class GravityEngine_Sampler
{

    // sampler parameters
public:

    std::vector<GravityEngine_SamplerVoice*> voices;
    int sample_frames;

    // Conceptually this comes from a prompt I gave to Copilot, but then I rewrote it from scratch based on my understanding of the concepts.
    // It simply generates a waveform. Never call this indepentently please. Use BindsamplerToChannel in the engine instead.
    // GravityEngine_sampler* sampler : sampler object reference
    // SDL_AudioStream* stream : Audio stream that the sampler audio plays on
    // SDL_AudioSpec* spec : Audio spec to format the audio with
    // SDL_AudioDeviceID dev : Device the audio will play on
    // ChannelStates* state : Current state of the channel
    // bool* sampler_playing : Flag to indicate the thread has successfully finished
    static void GenerateAudio(GravityEngine_Sampler* sampler, SDL_AudioStream* stream, SDL_AudioSpec* spec, SDL_AudioDeviceID dev, std::atomic<ChannelStates>* state, std::atomic<bool>* sampler_playing)
    {
        // Get sample frames
        SDL_GetAudioDeviceFormat(dev, spec, &sampler->sample_frames);
        // Initialize a random number generator
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> distrib(-10000, 10000);
        // Get buffer size
        int buffer_frames = sampler->sample_frames;
        int buffer_samples = buffer_frames * spec->channels;
        int buffer_bytes = buffer_samples * sizeof(float);
        float* buffer = (float*)SDL_malloc(buffer_bytes);
        bool first = true;

        SDL_SetAudioStreamGain(stream, 1.0f);

        // Keep supplying data
        while ((*state) == playing || (*state) == paused) {

            // If the sampler is paused, do not play the sampler
            if ((*state) == paused)
            {
                std::this_thread::yield();
                continue;
            }

            // Get the available stream in frames
            // ----------------------------------
            // A sample is one decimal. For mono that would be 1 sample per frame. 
            // However in Stereo, it's 1 sample per speaker per frame. 
            // So that would be 2 samples per frame.
            // This is why we are looping frame-by-frame. 
            // We are calculating all samples per frame in one loop cycle.
            int threshold_frames = sampler->sample_frames;
            int get_frames = sampler->sample_frames;
            //printf("%d\n", sampler->sample_frames);
            int available_frames = SDL_GetAudioStreamAvailable(stream) / (sizeof(float) * spec->channels);

            // Check available data
            if (available_frames < threshold_frames)
            {
                // Fill in audio data
                SDL_memset(buffer, 0, buffer_bytes);
                for (int frame = 0; frame < get_frames; frame++)
                {
                    for (auto v : sampler->voices)
                    {
                        if (v->will_start_playing)
                        {
                            v->freq = v->stg_base_freq.load();
                            v->volume = v->stg_volume.load();
                            v->panning = v->stg_panning.load();
                            v->pulse_width = v->stg_pulse_width.load();
                            v->stg_swpd_freq = v->stg_base_freq.load();

                            // Crop panning
                            v->panning = std::clamp<float>(v->panning, 0.f, 1.f);

                            v->pan_phase = v->panning;
                            v->pw_phase = v->pulse_width;

                            v->hp_l = 0.0f;
                            v->bp_l = 0.0f;
                            v->lp_l = 0.0f;
                            v->hp_r = 0.0f;
                            v->bp_r = 0.0f;
                            v->lp_r = 0.0f;

                            v->will_start_playing = false;
                            v->start_playing = true;
                        }

                        // Get any changes in my mailbox
                        if (v->frame_counter >= v->frames_per_tick && v->start_playing)
                        {
                            v->frame_counter -= v->frames_per_tick;
                            live_change_sample change;
                            // Get next in queue
                            if (v->live_changes.pop(change) && v->start_playing)
                            {
                                // Get any changes
                                if (change.freq != -9999)
                                    v->freq = change.freq;
                                if (change.pan != -2)
                                    v->panning = change.pan;
                                if (change.pw != -1)
                                    v->pulse_width = change.pw;
                                if (change.vol != -1)
                                    v->volume = change.vol;
                            }
                        }
                        v->frame_counter++;

                        // Only add this to the frame if it's playing
                        if (v->start_playing)
                        {
                            // The pitch of the sound is determined by sound wave cycles
                            // per second. Thus, we take the number of samples in a second
                            // And divide the pitch frequency across sample rate.
                            // Every time we get an audio frame, we add the pitch/number of samples
                            // to the phase to move forward at the proper rate to make that sound freq.
                            // We make the range of this phase 0 to 1. The range of
                            // a trig function input is 0 to 2PI. So we take the phase
                            // and map it to the cycle of the trig function by multiplying
                            // it by 2PI.
                            // Basically, phase is the normalized position in the cycle. 
                            // A cycle of a wave is 0 to 2PI.
                            // The faster the phase moves, the faster the wave cycles, and the higher the pitch.
                            // Phase is normalized because 2PI and 0 are the same position on a wave in trig.
                            float one = v->phase * 2. * PI;

                            // Set sample based on wave form
                            float sample = 0.;
                            if (v->waveform == SampleWaveForm::sample_sine)
                                sample = sin(one);
                            else if (v->waveform == SampleWaveForm::sample_square)
                                sample = (sin(one) > 0 ? 1 : -1);
                            else if (v->waveform == SampleWaveForm::sample_pulse)
                                sample = (sin(one) > v->pulse_width ? 1 : -1);
                            else if (v->waveform == SampleWaveForm::sample_sawtooth)
                                sample = (v->phase * 2.f - 1.f);
                            else if (v->waveform == SampleWaveForm::sample_triangle) // Source: https://en.wikipedia.org/wiki/Triangle_wave
                                sample = (((acos(cos(one + PI / 2)) * 2) / PI) - 1);
                            else if (v->waveform == SampleWaveForm::sample_noise)
                                sample = (distrib(gen) / 10000.);

                            // Apply panning volume and global volume
                            // In mono 0.5 = 1, 0 = 0.5, 1 = 0.5. 
                            // That way, panning still effects the audio output in mono.
                            // This is how the Gameboy does panning on its mono speaker.
                            float this_pan = (v->panning - 0.5f) * 2.0f;
                            float angle = (this_pan + 1.0f) * 0.5f * static_cast<float>(PI / 2);
                            float left_gain = std::cos(angle);
                            float right_gain = std::sin(angle);
                            float left_pan = spec->channels == 2 ?
                                left_gain : 1 - abs(0.5 - v->panning);
                            float right_pan = right_gain;
                            auto left_sample = left_pan * (v->volume) * sample;
                            auto right_sample = right_pan * (v->volume) * sample;

                            // Process filtered out
                            if (v->algorithm == SampleFilterAlgorithm::chamberlain)
                            {
                                ProcessChamberlainFilter(left_sample, v->cutoff, v->resonance, spec->freq, &v->lp_l, &v->bp_l, &v->hp_l);
                                ProcessChamberlainFilter(right_sample, v->cutoff, v->resonance, spec->freq, &v->lp_r, &v->bp_r, &v->hp_r);
                            }

                            // Get filtered value based on the type of filter
                            float filtered_left = (v->filter == SampleFilterType::lowpass ? v->lp_l : (v->filter == SampleFilterType::highpass ? v->hp_l : (v->filter == SampleFilterType::bandpass ? v->bp_l : left_sample)));

                            // Get filtered value based on the type of filter
                            float filtered_right = (v->filter == SampleFilterType::lowpass ? v->lp_r : (v->filter == SampleFilterType::highpass ? v->hp_r : (v->filter == SampleFilterType::bandpass ? v->bp_r : right_sample)));

                            // Fill the buffer differently depending on channel
                            if (spec->channels == 1)
                                buffer[frame] += filtered_left;
                            else
                            {
                                // Every frame is made up of a left sample and a right sample.
                                // We place the left sample into the buffer.
                                // Then the right sample.
                                buffer[frame * 2 + 0] += filtered_left;
                                buffer[frame * 2 + 1] += filtered_right;
                            }

                            // Step
                            v->phase += v->freq / spec->freq;
                            // Normalize phase
                            if (v->phase > 1.)
                            {
                                v->phase -= 1.;
                            }
                        }
                    }
                }

                // Push buffer to stream
                SDL_PutAudioStreamData(stream, buffer, buffer_bytes);
            }

            // Start the sampler playback but only if this is the first time starting
            if (first == true)
            {
                // Attach the audio stream to the channel's audio device
                SDL_BindAudioStream(dev, stream);
                // Start playback
                SDL_ResumeAudioDevice(dev);
                // Audio is bound, don't do this again.
                first = false;
            }
        }
        // End sequence
        SDL_free(buffer);
        (*sampler_playing) = false;
    }

    // Automate the sampler modulation variables (Effected by call rate)
    void SampleAutomation(double* new_freq, double* new_pan, double* new_pw, double* new_vol, int voice_index)
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
        // Step pulse width
        if (voices[voice_index]->pulse_width_freq > 0)
        {
            voices[voice_index]->pw_phase += voices[voice_index]->pulse_width_freq / 100;
            (*new_pw) = (sin(voices[voice_index]->pw_phase * 2. * PI) / 2) * 0.99 + 0.5;
            if (voices[voice_index]->pw_phase > 1.)
                voices[voice_index]->pw_phase -= 1.;
            voices[voice_index]->stg_pulse_width = (*new_pw);
        }
        // Step note
        if (voices[voice_index]->pitch_freq != 0)
        {
            if (voices[voice_index]->pitch_freq > 0)
                voices[voice_index]->stg_swpd_freq = voices[voice_index]->stg_swpd_freq * (voices[voice_index]->pitch_freq + 1); // Stage the new freq change
            if (voices[voice_index]->pitch_freq < 0)
                voices[voice_index]->stg_swpd_freq = voices[voice_index]->stg_swpd_freq / (abs(voices[voice_index]->pitch_freq) + 1); // Stage the new freq change
            auto freq_swp_ofst = voices[voice_index]->stg_swpd_freq - voices[voice_index]->stg_base_freq;
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
