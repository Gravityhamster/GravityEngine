#include "GravityEngineSDL.h"
#include <math.h>

GravityEngine_Core* geptr;
GravityEngine_Synth* synptr;

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
    /*int ind = geptr->AddSound(".\\DrumBeat.wav");
    geptr->PlaySoundOnChannel(ind, 0, false);*/
    synptr = new GravityEngine_Synth();
    geptr->BindSynthToChannel(synptr, 0);
}

// Master pre code
void PreGameLoop()
{
    if (geptr->GetKeyState(SDL_SCANCODE_0))
        geptr->StopChannel(0);
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