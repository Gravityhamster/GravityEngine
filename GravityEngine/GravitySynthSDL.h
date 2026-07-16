#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <iostream>
#include <chrono>
#include <math.h>
#include <future>
#include <thread>
#include <vector>
#include <string>
#include <unordered_map>
#include <random>
#include <map>

#define PI 3.141592f

// TODO: Any effects related directly to instrument automation should be implemented directly into the synth (i.e. vibrato, pitchsweep, fadein, fadeout, etc.)

// Enum to define the current playback state of a sound channel
enum ChannelStates
{
    stopped,
    init,
    playing,
    paused,
    uninit
};

// Enum to define the wave forms on a synth
enum class SynthWaveForm
{
    sine,
    square,
    pulse,
    sawtooth,
    triangle,
    noise,
    min = sine,
    max = noise
};

// Conversion map for waveforms
std::map<SynthWaveForm, std::string> waveform_to_string = {
    {SynthWaveForm::sine, "SINE"},
    {SynthWaveForm::square, "SQUARE"},
    {SynthWaveForm::pulse, "PULSE"},
    {SynthWaveForm::sawtooth, "SAWTOOTH"},
    {SynthWaveForm::triangle, "TRIANGLE"},
    {SynthWaveForm::noise, "NOISE"}
};

// Enum to define the type of filter applied to audio channel
enum class FilterType
{
    lowpass,
    highpass,
    bandpass,
    none,
    min = lowpass,
    max = none
};

// Template for synth objects
class GravityEngine_Synth
{
public:

    // Synth parameters
    std::atomic<float> freq = 50.0;
    std::atomic<float> volume = 1;
    std::atomic<float> panning = 0.5;
    std::atomic<float> pulse_width = 0.5;

    std::atomic<float> pitch_freq = 0;
    std::atomic<float> volume_freq = 0;
    std::atomic<float> pan_freq = 0.0;
    std::atomic<float> pulse_width_freq = 0.0;

    // float vibrato_freq = 0; -- Not yet implemented
    // float vibrato_amp = 0; -- Not yet implemented
    int sample_frames;
    SynthWaveForm waveform = SynthWaveForm::sine;
    FilterType filter = FilterType::none;

    // Filter - COPILOT
    float cutoff = 0.5f; // 0.0 - 1.0 -- TODO: Determine usable range
    float resonance = 0.5f; // 0.0 - 1.0 -- TODO: Determine usable range

    // Filter state - COPILOT
    float lp_l = 0.0f;
    float bp_l = 0.0f;
    float hp_l = 0.0f;
    float lp_r = 0.0f;
    float bp_r = 0.0f;
    float hp_r = 0.0f;

    // Other state variables
    float pan_phase = 0.f;
    float pw_phase = 0.f;

    // Conceptually this comes from a prompt I gave to Copilot, but then I rewrote it from scratch based on my understanding of the concepts.
    // It simply generates a waveform. Never call this indepentently please. Use BindSynthToChannel in the engine instead.
    // GravityEngine_Synth* synth : Synth object reference
    // SDL_AudioStream* stream : Audio stream that the synth audio plays on
    // SDL_AudioSpec* spec : Audio spec to format the audio with
    // SDL_AudioDeviceID dev : Device the audio will play on
    // ChannelStates* state : Current state of the channel
    // bool* synth_playing : Flag to indicate the thread has successfully finished
    static void GenerateAudio(GravityEngine_Synth* synth, SDL_AudioStream* stream, SDL_AudioSpec* spec, SDL_AudioDeviceID dev, std::atomic<ChannelStates>* state, std::atomic<bool>* synth_playing)
    {
        // Crop panning
        synth->panning = std::clamp<float>(synth->panning, 0.f, 1.f);
        // Get sample frames
        SDL_GetAudioDeviceFormat(dev, spec, &synth->sample_frames);
        // Initialize a random number generator
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> distrib(-10000, 10000);
        // Get buffer size
        int buffer_frames = synth->sample_frames;
        int buffer_samples = buffer_frames * spec->channels;
        int buffer_bytes = buffer_samples * sizeof(float);
        float* buffer = (float*)SDL_malloc(buffer_bytes);
        float phase = 0.;
        float pan_phase = synth->panning;
        float pw_phase = synth->pulse_width;

        synth->hp_l = 0.0f;
        synth->bp_l = 0.0f;
        synth->lp_l = 0.0f;
        synth->hp_r = 0.0f;
        synth->bp_r = 0.0f;
        synth->lp_r = 0.0f;

        // Keep supplying data
        while ((*state) == playing || (*state) == paused) {

            // If the synth is paused, do not play the synth
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
            // This is why we are ooping frame-by-frame. 
            // We are calculating all samples per frame in one loop cycle.
            int threshold_frames = synth->sample_frames * 2;
            int available_frames = SDL_GetAudioStreamAvailable(stream) / (sizeof(float) * spec->channels);

            // Check available data
            if (available_frames < threshold_frames)
            {
                // Fill in audio data
                for (int frame = 0; frame < synth->sample_frames; frame++)
                {
                    // Get oscillator one value | Why 2PI?
                    float one = phase * 2. * PI;

                    // Set sample based on wave form
                    float sample = 0.;
                    if (synth->waveform == SynthWaveForm::sine)
                        sample = sin(one);
                    else if (synth->waveform == SynthWaveForm::square)
                        sample = (sin(one) > 0 ? 1 : -1);
                    else if (synth->waveform == SynthWaveForm::pulse)
                        sample = (sin(one) > synth->pulse_width ? 1 : -1);
                    else if (synth->waveform == SynthWaveForm::sawtooth)
                        sample = (phase * 2.f - 1.f);
                    else if (synth->waveform == SynthWaveForm::triangle) // Source: https://en.wikipedia.org/wiki/Triangle_wave
                        sample = (((acos(cos(one + PI / 2)) * 2) / PI) - 1);
                    else if (synth->waveform == SynthWaveForm::noise)
                        sample = (distrib(gen) / 10000.);

                    // Apply panning volume and global volume
                    // In mono 0.5 = 1, 0 = 0.5, 1 = 0.5. 
                    // That way, panning still effects the audio output in mono.
                    float left_pan = spec->channels == 2 ? (1.f - synth->panning) : 1 - abs(0.5 - synth->panning);
                    float right_pan = synth->panning;
                    auto left_sample = left_pan * (synth->volume) * sample;
                    auto right_sample = right_pan * (synth->volume) * sample;

                    // Apply filter - COPILOT
                    float cutoff_hz = synth->cutoff * (spec->freq * 0.5f);
                    float f = 2.0f * sinf(PI * cutoff_hz / spec->freq);
                    float q = synth->resonance;

                    // Calculate left filter - COPILOT
                    synth->hp_l = left_sample - synth->lp_l - q * synth->bp_l;
                    synth->bp_l = synth->bp_l + f * synth->hp_l;
                    synth->lp_l = synth->lp_l + f * synth->bp_l;

                    // Get filtered value based on the type of filter
                    float filtered_left = (synth->filter == FilterType::lowpass ? synth->lp_l : (synth->filter == FilterType::highpass ? synth->hp_l : (synth->filter == FilterType::bandpass ? synth->bp_l : left_sample)));

                    // Calculate right filter - COPILOT
                    synth->hp_r = right_sample - synth->lp_r - q * synth->bp_r;
                    synth->bp_r = synth->bp_r + f * synth->hp_r;
                    synth->lp_r = synth->lp_r + f * synth->bp_r;

                    // Get filtered value based on the type of filter
                    float filtered_right = (synth->filter == FilterType::lowpass ? synth->lp_r : (synth->filter == FilterType::highpass ? synth->hp_r : (synth->filter == FilterType::bandpass ? synth->bp_r : right_sample)));

                    // Fill the buffer differently depending on channel
                    if (spec->channels == 1)
                        buffer[frame] = filtered_left;
                    else
                    {
                        // Every frame is made up of a left sample and a right sample.
                        // We place the left sample into the buffer.
                        // Then the right sample.
                        buffer[frame * 2 + 0] = filtered_left;
                        buffer[frame * 2 + 1] = filtered_right;
                    }

                    // Step
                    phase += synth->freq / spec->freq;
                    // Normalize phase
                    if (phase > 1.)
                    {
                        phase -= 1.;
                    }
                }

                // Push buffer to stream
                SDL_PutAudioStreamData(stream, buffer, buffer_bytes);
            }

            // Yield CPU and prevent overfilling the audio buffer 
            std::this_thread::yield();

        }
        // End sequence
        SDL_free(buffer);
        (*synth_playing) = false;
    }

    // Automate the synth modulation variables (Effected by call rate)
    void SynthAutomation()
    {
        // Step panning
        if (pan_freq > 0)
        {
            pan_phase += pan_freq / 100;
            panning = (sin(pan_phase * 2. * PI) / 2) + 0.5;
            if (pan_phase > 1.)
                pan_phase -= 1.;
        }
        // Step pulse width
        if (pulse_width_freq > 0)
        {
            pw_phase += pulse_width_freq / 100;
            pulse_width = (sin(pw_phase * 2. * PI) / 2) * 0.99 + 0.5;
            if (pw_phase > 1.)
                pw_phase -= 1.;
        }
        // Step note
        if (pitch_freq != 0)
        {
            if (pitch_freq > 0)
                freq = freq * (pitch_freq + 1);
            if (pitch_freq < 0)
                freq = freq / (abs(pitch_freq) + 1);
        }
        // Step volumne
        if (volume_freq != 0)
            volume += volume_freq / 100;
        if (volume < 0)
            volume = 0;
    }
};
