#include "GravityEngineSDL.h"

GravityEngine_Core* geptr;
bool ply = false;
bool is_true_a;
bool was_true_a;
bool is_true_left;
bool was_true_left;
bool is_true_right;
bool was_true_right;

int p;

class player : public virtual GravityEngine_Object
{
    private:
        int x;
        int y;
	public:
        player() 
        {
            x = 96 / 2;
            y = 54 / 2;
        };
		~player() {};
		void begin_step() {};
		void step() 
        {
            geptr->DrawChar(x, y, geptr->entity, '@');
        };
		void end_step() {};
};

// Master pre code
void GameInit()
{
    for (int q = 44; q < 54; q++)
    {
        for (int i = 0; i < 96; i++)
        {
            geptr->DrawChar(i, q, geptr->background, 'O');
            geptr->DrawSetColor(i, q, geptr->background, { {0,255,0},{0,0,0} });
            geptr->SetCollisionValue(i, q, geptr->stat, 1);

        }
    }

    p = geptr->AddObject(new player());
}

// Master pre code
void PreGameLoop()
{
    was_true_a = is_true_a;
    is_true_a = geptr->GetKeyState(SDL_SCANCODE_Z);
    was_true_left = is_true_left;
    is_true_left = geptr->GetKeyState(SDL_SCANCODE_LEFT);
    was_true_right = is_true_right;
    is_true_right = geptr->GetKeyState(SDL_SCANCODE_RIGHT);
}

// Master post code
void PostGameLoop()
{
}

int main()
{
    // Init engine - 128x72 is generally the largest you can get and still maintain good performance
    GravityEngine_Core ge_inst = GravityEngine_Core("Game", "com.example.game", "1.0", 96, 54, 60, 1920, 1080, "./Ubuntu-B-1.ttf", 16);

    ge_inst.debug_mode = true; // Show debug overlay
    ge_inst.debug_complex = true; // Show all information
    geptr = &ge_inst; // Set the pointer to the console engine class

    // Start game loop
    ge_inst.Start(&GameInit, &PreGameLoop, &PostGameLoop);

    // Report success to host
    return 0;
}