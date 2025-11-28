#include "GravityEngineSDL.h"
#include <algorithm>
GravityEngine_Core* geptr;

#define PI 3.14159265359

struct vertex2D
{
    double x;
    double y;
};

struct vertex3D
{
    double x;
    double y;
    double z;
};

void Translate2D(vertex2D* p, double deg, double len)
{
    double trans_x = len * cos(deg * (PI / 180));
    double trans_y = len * sin(deg * (PI / 180));
    p->x += trans_x;
    p->y -= trans_y;
}

void DrawLine3D(vertex2D origin, vertex3D p1, vertex3D p2, GravityEngine_Core::layer l, color col, char c, vertex3D quat = { 0, 0, 0 })
{
    vertex2D start = { origin.x, origin.y };
    vertex2D end = { origin.x, origin.y };

    double angle_pos_x = 330;
    double angle_pos_z = 210;
    double angle_pos_y = 90;
    
    Translate2D(&start, angle_pos_x, p1.x);
    Translate2D(&start, angle_pos_y, p1.y);
    Translate2D(&start, angle_pos_z, p1.z);

    Translate2D(&end, angle_pos_x, p2.x);
    Translate2D(&end, angle_pos_y, p2.y);
    Translate2D(&end, angle_pos_z, p2.z);

    geptr->DrawLine(start.x, start.y, end.x, end.y, l, col, c);
    geptr->DrawChar(origin.x, origin.y, l, c);
    geptr->DrawSetColor(origin.x, origin.y, l, { {255, 0, 0}, {0, 0, 0} });
}

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

int i = 0;

// Master pre code
void PreGameLoop()
{
    vertex3D quat = { 0, i++, 0 };
    DrawLine3D({ (double)geptr->GetCanvasW() / 2, (double)geptr->GetCanvasH() / 2 }, { {0}, {-10}, {0} }, { {10}, {10-10}, {-10} }, GravityEngine_Core::entity, { {255, 255, 255}, {0, 0, 0} }, 'O', quat);
    DrawLine3D({ (double)geptr->GetCanvasW() / 2, (double)geptr->GetCanvasH() / 2 }, { {0}, {-10}, {0} }, { {-10}, {10-10}, {10} }, GravityEngine_Core::entity, { {255, 255, 255}, {0, 0, 0} }, 'O', quat);
}

// Master post code
void PostGameLoop()
{
}

int main()
{
    // Init engine - 128x72 is generally the largest you can get and still maintain good performance
    GravityEngine_Core ge_inst = GravityEngine_Core("Boiler Plate", "com.example.snake", "1.0", 96, 54, 480, 1920, 1080, "./Ubuntu-B-1.ttf", 16);

    ge_inst.debug_mode = true; // Show debug overlay
    ge_inst.debug_complex = false; // Show all information
    geptr = &ge_inst; // Set the pointer to the console engine class

    // Start game loop
    ge_inst.Start(&GameInit, &PreGameLoop, &PostGameLoop);

    // Report success to host
    return 0;
}
