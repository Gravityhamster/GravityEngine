#include "GravityEngineSDL.h"
GravityEngine_Core* geptr;

double xd = 30;
double yd = 30;
int di = 0;
double spd = 0.5;

// Master pre code
void GameInit()
{
	for (int q = 0; q < (*geptr).GetCanvasH(); q++)
	{
		for (int i = 0; i < (*geptr).GetCanvasW(); i++)
		{
				(*geptr).DrawChar(i, q, (*geptr).background, 'A');
				(*geptr).DrawSetColor(i, q, (*geptr).background, {{0,0,255}, {0,0,0}});
		}
	}
	for (int q = 0; q < (*geptr).GetCanvasH(); q++)
	{
		for (int i = 0; i < (*geptr).GetCanvasW(); i++)
		{
				(*geptr).DrawChar(i, q, (*geptr).ui, rand()%10 == 0 ? 'B' : ' ');
				(*geptr).DrawSetColor(i, q, (*geptr).ui, {{0,255,0}, {0,0,0}});
		}
	}
	for (int q = 0; q < (*geptr).GetCanvasH(); q++)
	{
		for (int i = 0; i < (*geptr).GetCanvasW(); i++)
		{
				(*geptr).DrawChar(i, q, (*geptr).foreground, rand()%5 == 0 ? 'C' : ' ');
				(*geptr).DrawSetColor(i, q, (*geptr).foreground, {{0,255,255}, {0,0,0}});
		}
	}
}

// Master pre code
void PreGameLoop()
{

    std::cout << (*geptr).fps_now() << "                   ";
    std::cout << "\r";
    (*geptr).DrawChar((int)xd, (int)yd, (*geptr).entity, '@');
    (*geptr).DrawSetColor((int)xd, (int)yd, (*geptr).entity, {{255,0,0}, {0,0,0}});

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
    GravityEngine_Core ge_inst = GravityEngine_Core("Boiler Plate", "com.example.gravity", "1.0", 96, 54, 6000, true, 1920, 1080);

    // You must include a list of the characters you are going to use if you use quality mode
    std::vector<GravityEngine_Core::glyph> g = {
    		{{255,0,0},'@'},
    		{{0,0,255},'A'},
    		{{0,255,0},'B'},
    		{{0,255,255},'C'}
    };
    std::string to_add = "DeltaTime:1234567890.EpsdconFrPSChxu";
    for (auto c : to_add)
    	g.push_back({{255,255,255},c});
    // Copy to the glyph buffer
    ge_inst.SetAllGlyphs(g);

    ge_inst.debug_mode = true; // Show debug overlay
    ge_inst.debug_complex = true; // Show all information
    geptr = &ge_inst; // Set the pointer to the console engine class

    // Start game loop
    ge_inst.Start(&GameInit, &PreGameLoop, &PostGameLoop);

    // Report success to host
    return 0;
}
