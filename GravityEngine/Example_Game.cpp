#include "GravityEngineSDL.h"

GravityEngine_Core* geptr;
GravityEngine_Synth* synptr2;
bool s2isplaying = 0;
double global_timer = 1;
bool was = 0;
bool is = 0;
int bpm = 60; // Beats per minute
int fps = 360; // Frame rate in hz
int tps = 6; // Ticks per step
double frametick = 0;

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

// Get note freq
double NoteFreq(int n)
{
    // https://superglobalcalculator.com/calculators/music/piano-key-frequency/
    return 440.0 * pow(2.0, (n - 49.0) / 12.0);
}

// Beats-per-minute to Tick frequency
// b : bpm
// f : fps
double BpmToFrametick(int b, int f)
{
    // 60 beats / 1 minute
    // 60 beats / 60 seconds
    // 1 beat / 1 second
    // 6 ticks / 60 frames
    // 0.1 tick / 1 frame
    // 1 tick every 10 frames

    double x = b;
    x /= 60.0; // bpm / 60 seconds = bps
    x *= tps; // 6 ticks per beat
    x /= f; // ticks per frame

    return x;
}

// Execute tick
void DoTick()
{
    geptr->DrawTextString(5, 6, geptr->entity, std::to_string(synptr2->freq), { {255,255,255},{0,0,0} });
    synptr2->volume = 1;
}

// Master pre code
void GameInit()
{
    synptr2 = new GravityEngine_Synth();
    //synptr2->pulse_width_freq = 0.5f;
    synptr2->freq = 261.63;
    synptr2->volume = 1;
    synptr2->volume_freq = -10;
    synptr2->waveform = triangle;
    geptr->BindSynthToChannel(synptr2, 0);
}

// Master pre code
void PreGameLoop()
{
    // Show frame step calculation
    geptr->DrawTextString(5, 5, geptr->entity, "TICKS PER FRAME: " + std::to_string(BpmToFrametick(bpm, fps)), { {255,0,255},{0,0,0} });
    frametick = BpmToFrametick(bpm, fps);

    // Handle music clock
    global_timer += frametick;
    if (global_timer >= 1)
    {
        DoTick();
        while (global_timer >= 1)
            global_timer -= 1;
    }
}

// Master post code
void PostGameLoop()
{
}

int main()
{
    // Init engine - 128x72 is generally the largest you can get and still maintain good performance
    GravityEngine_Core ge_inst = GravityEngine_Core("Game", "com.example.game", "1.0", 96/2, 54/2, fps, 1920, 1080, "./GameFont.ttf", 64);

    ge_inst.debug_mode = true; // Show debug overlay
    ge_inst.debug_complex = false; // Show all information
    geptr = &ge_inst; // Set the pointer to the console engine class

    // Start game loop
    ge_inst.Start(&GameInit, &PreGameLoop, &PostGameLoop);

    // Report success to host
    return 0;
}