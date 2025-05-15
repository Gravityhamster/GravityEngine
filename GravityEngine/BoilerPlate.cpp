#include "GravityEngineSDL.h"
GravityEngine_Core* geptr;

double x = 0;
double y = 1;
double spd = 0.5;

// Master pre code
void GameInit()
{

}

// Master pre code
void PreGameLoop()
{
    std::cout << (*geptr).fps_now() << "                   ";
    std::cout << "\r";
    //int c = 0;
    //for (int i = 0; i < (*geptr).GetCanvasW(); i++)
    //{
    //    for (int q = 0; q < (*geptr).GetCanvasH(); q++)
    //    {
    //        if (i < (*geptr).GetCanvasW()/2 && q < (*geptr).GetCanvasH()/2)
    //        {
    //            (*geptr).DrawChar(i,q,(*geptr).ui,(*geptr).GetElapsedFrames());
    //            (*geptr).DrawSetColor(i,q,(*geptr).ui,c);
    //        }
    //        else if (i >= (*geptr).GetCanvasW()/2 && q < (*geptr).GetCanvasH()/2)
    //        {
    //            (*geptr).DrawChar(i,q,(*geptr).foreground,(*geptr).GetElapsedFrames());
    //            (*geptr).DrawSetColor(i,q,(*geptr).foreground,c);
    //        }
    //        else if (i < (*geptr).GetCanvasW()/2 && q >= (*geptr).GetCanvasH()/2)
    //        {
    //            (*geptr).DrawChar(i,q,(*geptr).background,(*geptr).GetElapsedFrames());
    //            (*geptr).DrawSetColor(i,q,(*geptr).background,c);
    //        }
    //        else if (i >= (*geptr).GetCanvasW()/2 && q >= (*geptr).GetCanvasH()/2)
    //        {
    //            (*geptr).DrawChar(i,q,(*geptr).entity,(*geptr).GetElapsedFrames());
    //            (*geptr).DrawSetColor(i,q,(*geptr).entity,c);
    //        }
    //        c++;
    //    }
    //}
    /*if ((*geptr).GetKey(VK_LEFT))
        x-=spd;
    if ((*geptr).GetKey(VK_RIGHT))
        x+=spd;
    if ((*geptr).GetKey(VK_UP))
        y-=spd;
    if ((*geptr).GetKey(VK_DOWN))
        y+=spd;*/
    (*geptr).DrawChar((int)x, (int)y, (*geptr).entity, '@');
    (*geptr).DrawSetColor((int)x, (int)y, (*geptr).background, (*geptr).GetElapsedFrames());
}

// Master post code
void PostGameLoop()
{
}

int main()
{
    // Init engine
    GravityEngine_Core ge_inst = GravityEngine_Core("Boiler Plate", "com.example.gravity", "1.0", 192, 108, 60);
    ge_inst.debug_mode = true; // Show debug overlay
    ge_inst.debug_complex = true; // Shwo all infor
    geptr = &ge_inst; // Set the pointer to the console engine class

    // Start game loop
    ge_inst.Start(&GameInit, &PreGameLoop, &PostGameLoop);

    // Report success to host
    return 0;
}
