#include "GravityEngineSDL.h"
#include <algorithm>
GravityEngine_Core* geptr;

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
}

int score = 0;

// Master pre code
void PreGameLoop()
{
    int x, y;
    char glyph = '>';
    if (geptr->GetMouseWheelState() > 0)
    {
        glyph = '^';
        score++;
    }
    if (geptr->GetMouseWheelState() < 0)
    {
        glyph = 'V';
        score--;
    }
    geptr->GetMousePosition(&x, &y);
    geptr->DrawChar(x, y, geptr->entity, glyph);
    geptr->DrawTextString(x + 1, y, geptr->entity, std::to_string(score), { {255,255,255} , {0,0,0} });
    if (geptr->GetMouseButtonState(SDL_BUTTON_LEFT))
    {
        geptr->DrawSetColor(x, y, geptr->entity, { {0,0,0} , {255,255,255} });
        geptr->DrawSetColor(x, y, geptr->background, { {0,0,0} , {255,255,255} });
    }
    if (geptr->GetMouseButtonState(SDL_BUTTON_RIGHT))
    {
        geptr->DrawSetColor(x, y, geptr->entity, { {255,255,255} , {0,0,0} });
        geptr->DrawSetColor(x, y, geptr->background, { {255,255,255} , {0,0,0} });
    }
}

// Master post code
void PostGameLoop()
{
}

int main()
{
    // Init engine - 128x72 is generally the largest you can get and still maintain good performance
    GravityEngine_Core ge_inst = GravityEngine_Core("Mouse Example", "com.example.mouse", "1.0", 96, 54, 6000, 1920, 1080, "./Ubuntu-B-1.ttf", 16);

    ge_inst.debug_mode = true; // Show debug overlay
    ge_inst.debug_complex = false; // Show all information
    geptr = &ge_inst; // Set the pointer to the console engine class

    // Start game loop
    ge_inst.Start(&GameInit, &PreGameLoop, &PostGameLoop);

    // Report success to host
    return 0;
}
