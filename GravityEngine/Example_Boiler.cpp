#include "GravityEngineSDL.h"
#include <math.h>

GravityEngine_Core* geptr;
bool ply = false;
bool is_true;
bool was_true;

GravityEngine_Synth* synptr;
GravityEngine_Synth* synptr1;
GravityEngine_Synth* synptr2;
GravityEngine_Synth* synptr3;
GravityEngine_Synth* synptr4;

class example : public virtual GravityEngine_Object
{
    private:
	public:
        example() {};
		~example() {};
		void begin_step() {};
		void step() {};
		void end_step() {};
};

// Master pre code
void GameInit()
{
    synptr = new GravityEngine_Synth();
    synptr->freq = 261.63;
    synptr->pulse_width_delta = 0;
    synptr->volume = 0.125 / 10;

    synptr1 = new GravityEngine_Synth();
    synptr1->freq = 261.63 - 1;
    synptr->pulse_width_delta = 1;
    synptr1->volume = 0.125 / 10;

    synptr2 = new GravityEngine_Synth();
    synptr2->freq = 261.63 + 1;
    synptr->pulse_width_delta = 2;
    synptr2->volume = 0.125 / 10;

    synptr3 = new GravityEngine_Synth();
    synptr3->freq = 261.63 - 2;
    synptr->pulse_width_delta = 3;
    synptr3->volume = 0.125 / 10;

    synptr4 = new GravityEngine_Synth();
    synptr4->freq = 261.63 + 2;
    synptr->pulse_width_delta = 4;
    synptr4->volume = 0.125 / 10;
}

void play()
{
    geptr->BindSynthToChannel(synptr, 0);
    geptr->BindSynthToChannel(synptr1, 1);
    geptr->BindSynthToChannel(synptr2, 2);
    geptr->BindSynthToChannel(synptr3, 3);
    geptr->BindSynthToChannel(synptr4, 4);
}

// Master pre code
void PreGameLoop()
{
    was_true = is_true;
    is_true = geptr->GetKeyState(SDL_SCANCODE_0);

    if (!is_true && ply)
    {
        geptr->StopChannel(0);
        geptr->StopChannel(1);
        geptr->StopChannel(2);
        geptr->StopChannel(3);
        geptr->StopChannel(4);
        ply = false;
    }
    else if (is_true && !ply)
    {
        play();
        ply = true;
    }
}

// Master post code
void PostGameLoop()
{
}

int main()
{
    // Init engine - 128x72 is generally the largest you can get and still maintain good performance
    GravityEngine_Core ge_inst = GravityEngine_Core("Boiler Plate", "com.example.snake", "1.0", 96, 54, 60, 1920, 1080, "./Ubuntu-B-1.ttf", 16);

    ge_inst.debug_mode = true; // Show debug overlay
    ge_inst.debug_complex = false; // Show all information
    geptr = &ge_inst; // Set the pointer to the console engine class

    // Start game loop
    ge_inst.Start(&GameInit, &PreGameLoop, &PostGameLoop);

    // Report success to host
    return 0;
}

//int main()
//{
//    SDL_Init(SDL_INIT_AUDIO);
//
//    SDL_AudioDeviceID dev =
//        SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
//
//    SDL_AudioSpec spec;
//    int sample_frames;
//
//    SDL_GetAudioDeviceFormat(dev, &spec, &sample_frames);
//
//    SDL_AudioStream* stream = SDL_CreateAudioStream(&spec, &spec);
//    SDL_BindAudioStream(dev, stream);
//
//    float freq = 200.0f;
//    const float sample_rate = (float)spec.freq;
//
//    // Use the device's preferred frame size
//    int samples = sample_frames * spec.channels;
//
//    float* buffer = (float*)SDL_malloc(samples * sizeof(float));
//    float phase = 0.0f;
//
//    double q = 0.0;
//    double v = 0.125 / 2;
//
//    while (1) {
//        for (int i = 0; i < samples; i++) {
//            auto one = phase * 2.0f * 3.141592f;
//            // buffer[i] = v*(sinf(one) > 0 ? 1 : -1); // SQUARE
//            // buffer[i] = v*(sinf(one) > (sin(q)/2)+0.5 ? 1 : -1); // PULSE
//            buffer[i] = sinf(one);
//            // buffer[i] = fmod(one, 1);
//            phase += freq / sample_rate;
//            if (phase >= 1.0f) phase -= 1.0f;
//        }
//
//        SDL_PutAudioStreamData(stream, buffer, samples * sizeof(float));
//        SDL_Delay(5);
//
//        //freq = 400.0f + sin(q)*100;
//        q += 1. / 20.;
//    }
//
//    SDL_free(buffer);
//    SDL_DestroyAudioStream(stream);
//    SDL_CloseAudioDevice(dev);
//    SDL_Quit();
//}