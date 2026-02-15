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

#define PI 3.141592f

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
enum SynthWaveForm
{
    sine,
    square,
    pulse,
    sawtooth,
    triangle,
    noise
};

/*
// Enum to define the type of filter applied to audio channel
enum FilterType
{
    lowpass,
    highpass,
    bandpass,
    comb
};

// Structure to define a filter for the audio channels
struct GravityEngine_Filter
{
    FilterType type;
    float frequency;
    float slope;
    float resonance;
    float bandpass_plateu;
};
*/

// Template for synth objects
class GravityEngine_Synth
{
public:
    float freq = 50.0;
    int sample_frames;
    float* audio_data;
    float volume = 1;
    float panning = 0.5;
    float pan_freq = 0.0;
    float pulse_width = 0.5;
    float pulse_width_freq = 0.0;
    SynthWaveForm waveform = sine;

    // Conceptually this comes from a prompt I gave to Copilot, but then I rewrote it from scratch based on my understanding of the concepts.
    // It simply generates a waveform. Never call this indepentently please. Use BindSynthToChannel in the engine instead.
    // GravityEngine_Synth* synth : Synth object reference
    // SDL_AudioStream* stream : Audio stream that the synth audio plays on
    // SDL_AudioSpec* spec : Audio spec to format the audio with
    // SDL_AudioDeviceID dev : Device the audio will play on
    // ChannelStates* state : Current state of the channel
    // bool* synth_playing : Flag to indicate the thread has successfully finished
    static void GenerateAudio(GravityEngine_Synth* synth, SDL_AudioStream* stream, SDL_AudioSpec* spec, SDL_AudioDeviceID dev, ChannelStates* state, bool* synth_playing)
    {
        // Crop panning
        synth->panning = std::clamp<float>(synth->panning, 0.f, 1.f);
        // Get sample frames
        SDL_GetAudioDeviceFormat(dev, spec, &synth->sample_frames);
        if (spec->channels > 2)
            spec->channels = 2;
        // Get buffer size
        int buffer_size = synth->sample_frames * spec->channels;
        float* buffer = (float*)SDL_malloc(buffer_size * sizeof(float));
        float phase = 0.;
        float pan_phase = synth->panning;
        float pw_phase = synth->pulse_width;
        // Keep supplying data
        while ((*state) == playing || (*state) == paused) {
            // If the synth is paused, do not play the synth
            if ((*state) == paused)
            {
                SDL_Delay(0);
                continue;
            }
            // Fill in audio data
            for (int i = 0; i < buffer_size; i++)
            {
                if (spec->channels == 2)
                {
                    if (i % 2 == 0) // left
                    {
                        float pan_volume = abs(synth->panning - 1.);
                        float one = phase * 2. * PI;
                        // Set sample based on wave form
                        float sample = 0.;
                        if (synth->waveform == sine)
                            sample = (pan_volume * synth->volume) * sin(one);
                        else if (synth->waveform == square)
                            sample = (pan_volume * synth->volume) * (sin(one) > 0 ? 1 : -1);
                        else if (synth->waveform == pulse)
                            sample = (pan_volume * synth->volume) * (sin(one) > synth->pulse_width ? 1 : -1);
                        else if (synth->waveform == sawtooth)
                            sample = (pan_volume * synth->volume) * (one * 2 - 1);
                        else if (synth->waveform == triangle) // Source: https://en.wikipedia.org/wiki/Triangle_wave
                            sample = (pan_volume * synth->volume) * (((acos(cos(one + PI / 2)) * 2) / PI) - 1);
                        else if (synth->waveform == noise)
                        {
                            // Initialize a random number generator
                            std::random_device rd;
                            std::mt19937 gen(rd());
                            std::uniform_int_distribution<> distrib(-10000, 10000);
                            // Return value
                            sample = distrib(gen) / 10000.;
                        }

                        buffer[i] = sample;
                    }
                    if (i % 2 == 1) // right
                    {
                        float pan_volume = 1 - abs(synth->panning - 1.);
                        float one = phase * 2. * PI;
                        // Set sample based on wave form
                        float sample = 0.;
                        if (synth->waveform == sine)
                            sample = (pan_volume * synth->volume) * sin(one);
                        else if (synth->waveform == square)
                            sample = (pan_volume * synth->volume) * (sin(one) > 0 ? 1 : -1);
                        else if (synth->waveform == pulse)
                            sample = (pan_volume * synth->volume) * (sin(one) > synth->pulse_width ? 1 : -1);
                        else if (synth->waveform == sawtooth)
                            sample = (pan_volume * synth->volume) * (one * 2 - 1);
                        else if (synth->waveform == triangle) // Source: https://en.wikipedia.org/wiki/Triangle_wave
                            sample = (pan_volume * synth->volume) * (((acos(cos(one + PI / 2)) * 2) / PI) - 1);
                        else if (synth->waveform == noise)
                        {
                            // Initialize a random number generator
                            std::random_device rd;
                            std::mt19937 gen(rd());
                            std::uniform_int_distribution<> distrib(-10000, 10000);
                            // Return value
                            sample = distrib(gen) / 10000.;
                        }

                        buffer[i] = sample;
                        // Step
                        phase += synth->freq / spec->freq;
                        if (phase > 1.)
                        {
                            phase -= 1.;
                        }
                        // Step panning
                        if (synth->pan_freq > 0)
                        {
                            pan_phase += synth->pan_freq / spec->freq;
                            synth->panning = (sin(pan_phase * 2. * PI) / 2) + 0.5;
                            if (pan_phase > 1.)
                                pan_phase -= 1.;
                        }
                        // Step pulse width
                        if (synth->pulse_width_freq > 0)
                        {
                            pw_phase += synth->pulse_width_freq / spec->freq;
                            synth->pulse_width = (sin(pw_phase * 2. * PI) / 2) * 0.99 + 0.5;
                            if (pw_phase > 1.)
                                pw_phase -= 1.;
                        }
                    }
                }
                else
                {
                    float one = phase * 2. * PI;
                    float sample = synth->volume * sin(one);
                    buffer[i] = sample;
                    if (phase > 1.)
                        phase -= 1.;
                }
            }
            // Push buffer to stream
            SDL_PutAudioStreamData(stream, buffer, buffer_size * sizeof(float));
            // Yield CPU and prevent overfilling the audio buffer - Note; this came from Copilot, and I added the part of the condition that handles play and pause
            while (SDL_GetAudioStreamAvailable(stream) > buffer_size && ((*state) == playing || (*state) == paused)) {
                SDL_Delay(0); // yield without adding latency 
            }
        }
        // End sequence
        SDL_free(buffer);
        (*synth_playing) = false;
    }
};