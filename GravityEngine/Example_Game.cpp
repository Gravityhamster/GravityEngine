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
        double x;
        double y;
        double yvel = 0;
        double xvel = 0;
        double grav = .0625/4;
        double max_grav = 5;
        double g_accel = 0.25;
        double a_accel = 0.25/5;
        double g_deccel = 0.25/10;
        double a_deccel = 0.25/20;
        double spd = 0.25;
        double jump = -0.5;
        double minjump = -0.25;
        double coyote_time = 10;
        double coyote_timer = 0;
        bool willjump = false;
        int willjump_time = 10;
        int willjump_timer = 0;
        std::vector<int> collides_with = {1};
	public:
        player() 
        {
            x = geptr->GetCanvasW() / 2;
            y = geptr->GetCanvasH() / 2;
        };
		~player() {};
		void begin_step() {};
		void step() 
        {
            // Calculate velocities
            if (!check_collision_solid(floor(x), floor(y + 1)))
            {
                willjump_timer++;
                coyote_timer++;
                if (yvel < max_grav)
                    yvel += grav;
            }
            else 
            {
                // Reset coyote time and velocity
                coyote_timer = 0;
                if (yvel > 0)
                    yvel = 0;
            }

            // Handle input
            double accel = check_collision_solid(floor(x), floor(y + 1)) ? g_accel : a_accel;
            double deccel = check_collision_solid(floor(x), floor(y + 1)) ? g_deccel : a_deccel;

            xvel += (is_true_right - is_true_left) * accel;
            if ((is_true_right - is_true_left) == 0)
            {
                if (xvel > 0)
                {
                    if (xvel - deccel < 0)
                        xvel = 0;
                    else
                        xvel -= deccel;
                }
                if (xvel < 0)
                {
                    if (xvel + deccel > 0)
                        xvel = 0;
                    else
                        xvel += deccel;
                }
            }
            xvel = std::clamp(xvel, -spd, spd);
            if (!is_true_a)
                yvel = std::max(yvel, minjump);
            if ((check_collision_solid(floor(x), floor(y + 1)) || coyote_timer < coyote_time) && ((is_true_a && !was_true_a) || willjump))
            {
                yvel = jump;
                willjump = false;
            }
            else if (is_true_a && !was_true_a)
            {
                willjump = true;
                willjump_timer = 0;
            }
            if (willjump_timer >= willjump_time)
                willjump = false;

            // Set the temp y and x velocity movement values
            double xv_t = xvel;
            double yv_t = yvel;

            // Apply the x velocity
            if (check_collision_solid_x(floor(x + xv_t)))
            {
                while (abs(xv_t) >= 1 && !check_collision_solid(floor(x), floor(x + (xv_t / abs(xv_t)))))
                {
                    x += xv_t / abs(xv_t);
                    xv_t = (abs(xv_t) - 1) * (xv_t / abs(xv_t));
                }
                xv_t = 0;
                xvel = 0;
            }
            x += xv_t;

            // Apply the y velocity
            if (check_collision_solid_y(floor(y + yv_t)))
            {
                while (abs(yv_t) >= 1 && !check_collision_solid(floor(x), floor(y + (yv_t / abs(yv_t)))))
                {
                    y += yv_t / abs(yv_t);
                    yv_t = (abs(yv_t) - 1) * (yv_t / abs(yv_t));
                }
                yv_t = 0;
                yvel = 0;
            }
            y += yv_t;

            // Draw the character at the end
            geptr->DrawChar(floor(x), floor(y), geptr->entity, '{');
        };
		void end_step() {};

        // Check if there is a collision at this point
        bool check_collision_solid(int _x, int _y)
        {
            if (_x == x && _y == y)
                return false;
            else
                return (VectorContains(collides_with, geptr->GetCollisionValue(_x, _y, geptr->stat)) ||
                        VectorContains(collides_with, geptr->GetCollisionValue(_x, _y, geptr->dyn)));
        }

        // Check if there is a collision at any point between here and there horizontally
        bool check_collision_solid_x(int _x)
        {
            if (_x == x)
                return false;
            else
            {
                while (abs(_x - floor(x)) >= 1)
                {
                    if (check_collision_solid(_x, y))
                        return true;
                    if (_x < x)
                        _x++;
                    else if (_x > x)
                        _x--;
                }
                return false;
            }
        }

        // Check if there is a collision at eny point between here and there vertically
        bool check_collision_solid_y(int _y)
        {
            if (_y == y)
                return false;
            else
            {
                while (abs(_y - floor(y)) >= 1)
                {
                    if (check_collision_solid(x, _y))
                        return true;
                    if (_y < y)
                        _y++;
                    else if (_y > y)
                        _y--;
                }
                return false;
            }
        }
};

// Master pre code
void GameInit()
{
    for (int q = 0; q < geptr->GetCanvasH(); q++)
    {
        for (int i = 0; i < geptr->GetCanvasW(); i++)
        {
            geptr->DrawChar(i, q, geptr->background, ' ');
            geptr->DrawSetColor(i, q, geptr->background, { {0,0,0},{0,0,255} });
        }
    }

    int q = geptr->GetCanvasH() - 1;
    for (int i = 0; i < geptr->GetCanvasW(); i++)
    {
        geptr->DrawChar(i, q, geptr->background, 'A');
        geptr->DrawSetColor(i, q, geptr->background, { {0,255,0},{0,255,0} });
        geptr->SetCollisionValue(i, q, geptr->stat, 1);
    }

    p = geptr->AddObject(new player());
}

// Master pre code
void PreGameLoop()
{
    was_true_a = is_true_a;
    is_true_a = geptr->GetKeyState(SDL_SCANCODE_UP);
    was_true_left = is_true_left;
    is_true_left = geptr->GetKeyState(SDL_SCANCODE_LEFT);
    was_true_right = is_true_right;
    is_true_right = geptr->GetKeyState(SDL_SCANCODE_RIGHT);

    if (geptr->GetMouseButtonState(SDL_BUTTON_LEFT))
    {
        int _x;
        int _y;
        geptr->GetMousePosition(&_x, &_y);

        geptr->DrawChar(_x, _y, geptr->foreground, 'B');
        geptr->DrawSetColor(_x, _y, geptr->foreground, { {0,255,0},{0,255,0} });
        geptr->SetCollisionValue(_x, _y, geptr->stat, 1);
    }
}

// Master post code
void PostGameLoop()
{
}

int main()
{
    // Init engine - 128x72 is generally the largest you can get and still maintain good performance
    GravityEngine_Core ge_inst = GravityEngine_Core("Game", "com.example.game", "1.0", 96/2, 54/2, 9999, 1920, 1080, "./GameFont.ttf", 16);

    ge_inst.debug_mode = true; // Show debug overlay
    ge_inst.debug_complex = false; // Show all information
    geptr = &ge_inst; // Set the pointer to the console engine class

    // Start game loop
    ge_inst.Start(&GameInit, &PreGameLoop, &PostGameLoop);

    // Report success to host
    return 0;
}