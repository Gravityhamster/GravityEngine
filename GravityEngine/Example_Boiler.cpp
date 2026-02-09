#include "GravityEngineSDL.h"

GravityEngine_Core* geptr;
bool ply = false;
bool is_true_c;
bool was_true_c;
bool is_true_cs;
bool was_true_cs;
bool is_true_d;
bool was_true_d;
bool is_true_ds;
bool was_true_ds;
bool is_true_e;
bool was_true_e;
bool is_true_f;
bool was_true_f;
bool is_true_fs;
bool was_true_fs;
bool is_true_g;
bool was_true_g;
bool is_true_gs;
bool was_true_gs;
bool is_true_a;
bool was_true_a;
bool is_true_as;
bool was_true_as;
bool is_true_b;
bool was_true_b;

GravityEngine_Synth* synptr;
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

void play()
{
    geptr->BindSynthToChannel(std::ref(synptr), 0);
    geptr->BindSynthToChannel(std::ref(synptr2), 1);
    geptr->BindSynthToChannel(std::ref(synptr3), 2);
    geptr->BindSynthToChannel(std::ref(synptr4), 3);
}

// Master pre code
void GameInit()
{
    synptr = new GravityEngine_Synth();
    synptr->pulse_width_freq = 1.0f;
    synptr->freq = 261.63;
    synptr->volume = 0.25;
    synptr->waveform = sine;

    synptr2 = new GravityEngine_Synth();
    synptr2->pulse_width_freq = 1.0f;
    synptr2->freq = 261.63;
    synptr2->volume = 0.25;
    synptr2->waveform = square;

    synptr3 = new GravityEngine_Synth();
    synptr3->pulse_width_freq = 1.0f;
    synptr3->freq = 261.63;
    synptr3->volume = 0.25;
    synptr3->waveform = sawtooth;

    synptr4 = new GravityEngine_Synth();
    synptr4->pulse_width_freq = 1.0f;
    synptr4->freq = 261.63;
    synptr4->volume = 0.25;
    synptr4->waveform = triangle;
}

// Master pre code
void PreGameLoop()
{
    was_true_c = is_true_c;
    is_true_c = geptr->GetKeyState(SDL_SCANCODE_A);
    was_true_cs = is_true_cs;
    is_true_cs = geptr->GetKeyState(SDL_SCANCODE_W);
    was_true_d = is_true_d;
    is_true_d = geptr->GetKeyState(SDL_SCANCODE_S);
    was_true_ds = is_true_ds;
    is_true_ds = geptr->GetKeyState(SDL_SCANCODE_E);
    was_true_e = is_true_e;
    is_true_e = geptr->GetKeyState(SDL_SCANCODE_D);
    was_true_f = is_true_f;
    is_true_f = geptr->GetKeyState(SDL_SCANCODE_F);
    was_true_fs = is_true_fs;
    is_true_fs = geptr->GetKeyState(SDL_SCANCODE_T);
    was_true_g = is_true_g;
    is_true_g = geptr->GetKeyState(SDL_SCANCODE_G);
    was_true_gs = is_true_gs;
    is_true_gs = geptr->GetKeyState(SDL_SCANCODE_Y);
    was_true_a = is_true_a;
    is_true_a = geptr->GetKeyState(SDL_SCANCODE_H);
    was_true_as = is_true_as;
    is_true_as = geptr->GetKeyState(SDL_SCANCODE_U);
    was_true_b = is_true_b;
    is_true_b = geptr->GetKeyState(SDL_SCANCODE_J);

    bool is_true = is_true_c || is_true_cs || is_true_d || is_true_ds || is_true_e || is_true_f || is_true_fs || is_true_g || is_true_gs || is_true_a || is_true_as || is_true_b;

    if (is_true_c && !was_true_c)
        synptr->freq = 261.63;
    if (is_true_cs && !was_true_cs) 
        synptr->freq = 277.18;
    if (is_true_d && !was_true_d) 
        synptr->freq = 293.66;
    if (is_true_ds && !was_true_ds) 
        synptr->freq = 311.13;
    if (is_true_e && !was_true_e) 
        synptr->freq = 329.63;
    if (is_true_f && !was_true_f) 
        synptr->freq = 349.23;
    if (is_true_fs && !was_true_fs) 
        synptr->freq = 369.99;
    if (is_true_g && !was_true_g)
        synptr->freq = 392.00;
    if (is_true_gs && !was_true_gs) 
        synptr->freq = 415.30;
    if (is_true_a && !was_true_a) 
        synptr->freq = 440.00;
    if (is_true_as && !was_true_as) 
        synptr->freq = 466.16;
    if (is_true_b && !was_true_b) 
        synptr->freq = 493.88;

    synptr2->freq = synptr->freq - 1;
    synptr3->freq = synptr->freq + 1;
    synptr4->freq = synptr->freq + 2;

    if (!is_true && ply)
    {
        geptr->StopChannel(0);
        geptr->StopChannel(1);
        geptr->StopChannel(2);
        geptr->StopChannel(3);
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