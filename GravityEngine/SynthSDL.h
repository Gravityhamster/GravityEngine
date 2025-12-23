#pragma once

// --== NOTE THAT THE FOLLOWING COMMENTED CODE IS THE ORIGINAL CODE I GENERATED FROM CO-PILOT ==--
//		This header seeks to implement this logic into a reusable class that can be played as
//		a musical instrument.
//// Entirely provided by copilot - Additional waveforms by me
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
//    float freq = 50.0f;
//    const float sample_rate = (float)spec.freq;
//
//    // Use the device's preferred frame size
//    int samples = sample_frames * spec.channels;
//
//    float* buffer = (float*)SDL_malloc(samples * sizeof(float));
//    float phase = 0.0f;
//
//    double q = 0.0;
//    double v = 0.125/2;
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
//        q += 1./20.;
//    }
//
//    SDL_free(buffer);
//    SDL_DestroyAudioStream(stream);
//    SDL_CloseAudioDevice(dev);
//    SDL_Quit();
//}

class synth
{

};

class oscillator
{

};