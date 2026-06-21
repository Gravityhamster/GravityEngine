#include "GravityEngineSDL.h"

GravityEngine_Core* geptr;

class player : public virtual GravityEngine_Object
{
    private:
	public:
        player() {};
		~player() {};
		void begin_step() {};
		void step() {};
		void end_step() {};
};

// Master pre code
void GameInit()
{
    auto s = new GravityEngine_Synth();
}

// Master pre code
void PreGameLoop()
{
}

// Master post code
void PostGameLoop()
{
}

int main()
{
    // Init engine - 128x72 is generally the largest you can get and still maintain good performance
    GravityEngine_Core ge_inst = GravityEngine_Core("Game", "com.example.game", "1.0", 96/2, 54/2, 60, 1920, 1080, "./GameFont.ttf", 64);

    ge_inst.debug_mode = true; // Show debug overlay
    ge_inst.debug_complex = false; // Show all information
    geptr = &ge_inst; // Set the pointer to the console engine class

    // Start game loop
    ge_inst.Start(&GameInit, &PreGameLoop, &PostGameLoop);

    // Report success to host
    return 0;
}