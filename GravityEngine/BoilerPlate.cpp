#include "GravityEngineSDL.h"
GravityEngine_Core* geptr;

double xd = 30;
double yd = 30;
int di = 0;
double spd = 0.5;
int snd_id;

class snake : public virtual GravityEngine_Object
{
	public:
        double x = 15;
        double y = 15;
		snake() {};
		~snake() {};
		void begin_step() {};
		void step()
		{
            (*geptr).DrawChar(x, y, (*geptr).entity, '@');
			(*geptr).DrawSetColor(x,y,(*geptr).entity, {{255,0,0}, {0,0,125}});
            
            if ((*geptr).GetKeyState(SDLK_RIGHT)) x += .5;
            if ((*geptr).GetKeyState(SDLK_LEFT)) x -= .5;
            if ((*geptr).GetKeyState(SDLK_UP)) y -= .5;
            if ((*geptr).GetKeyState(SDLK_DOWN)) y += .5;
		};
		void end_step() {};
};

// Master pre code
void GameInit()
{
    // Create a sound
    snd_id = (*geptr).AddSound(".\\DrumBeat.wav");
    (*geptr).PlaySoundOnChannel(snd_id, 0);

    (*geptr).AddObject(new snake);

	for (int q = 0; q < (*geptr).GetCanvasH(); q++)
	{
		for (int i = 0; i < (*geptr).GetCanvasW(); i++)
		{
				(*geptr).DrawChar(i, q, (*geptr).background, 'A');
				(*geptr).DrawSetColor(i, q, (*geptr).background, {{0,0,255}, {125,125,125}});
		}
	}
	for (int q = 0; q < (*geptr).GetCanvasH(); q++)
	{
		for (int i = 0; i < (*geptr).GetCanvasW(); i++)
		{
				(*geptr).DrawChar(i, q, (*geptr).ui, rand()%10 == 0 ? 'B' : ' ');
				(*geptr).DrawSetColor(i, q, (*geptr).ui, {{0,255,0}, {125,125,125}});
		}
	}
	for (int q = 0; q < (*geptr).GetCanvasH(); q++)
	{
		for (int i = 0; i < (*geptr).GetCanvasW(); i++)
		{
				(*geptr).DrawChar(i, q, (*geptr).foreground, rand()%5 == 0 ? 'C' : ' ');
				(*geptr).DrawSetColor(i, q, (*geptr).foreground, {{0,255,255}, {125,125,125}});
		}
	}
}

// Master pre code
void PreGameLoop()
{
    std::cout << (*geptr).fps_now() << "                   ";
    std::cout << "\r";
    (*geptr).DrawChar((int)xd, (int)yd, (*geptr).entity, '@');
    (*geptr).DrawSetColor((int)xd, (int)yd, (*geptr).entity, {{255,0,0}, {0,0,125}});

    if ((*geptr).GetElapsedFrames() % 1000 == 0)
    	(*geptr).ChangeFont("./Ubuntu-B-1.ttf");
    else if ((*geptr).GetElapsedFrames() % 500 == 0)
    	(*geptr).ChangeFont("./PixelPerfect.ttf");

    if (di == 0)
    	xd += spd;
    if (di == 1)
    	yd -= spd;
    if (di == 2)
    	xd -= spd;
    if (di == 3)
    	yd += spd;

    if (di == 0 && xd >= 50)
    	di = 1;
    if (di == 1 && yd <= 20)
    	di = 2;
    if (di == 2 && xd <= 20)
    	di = 3;
    if (di == 3 && yd >= 50)
    	di = 0;
}

// Master post code
void PostGameLoop()
{
}

int main()
{
    // Init engine - 128x72 is generally the largest you can get and still maintain good performance
    GravityEngine_Core ge_inst = GravityEngine_Core("Boiler Plate", "com.example.gravity", "1.0", 96, 54, 60, 1920, 1080, "./Ubuntu-B-1.ttf", 16);

    ge_inst.debug_mode = true; // Show debug overlay
    ge_inst.debug_complex = true; // Show all information
    geptr = &ge_inst; // Set the pointer to the console engine class

    // Start game loop
    ge_inst.Start(&GameInit, &PreGameLoop, &PostGameLoop);

    // Report success to host
    return 0;
}
