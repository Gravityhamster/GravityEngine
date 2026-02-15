#include "GravityEngineSDL.h"

GravityEngine_Core* geptr;
bool ply = false;
bool is_true_a;
bool was_true_a;
bool is_true_left;
bool was_true_left;
bool is_true_right;
bool was_true_right;
bool is_true_up;
bool was_true_up;
bool is_true_down;
bool was_true_down;

int p;

struct bounding_box
{
    int x;
    int y;
    int w;
    int h;
};

class player : public virtual GravityEngine_Object
{
    private:
        double x;
        double y;
        double yvel = 0;
        double xvel = 0;
        double grav = 0.015625;
        double max_grav = 5;
        double g_accel = 0.25;
        double a_accel = 0.05;
        double g_deccel = 0.025;
        double a_deccel = 0.0125;
        double spd = 0.25;
        double jump = -0.5;
        double minjump = -0.25;
        double coyote_time = 10;
        double coyote_timer = 0;
        bool willjump = false;
        int willjump_time = 10;
        int willjump_timer = 0;
        int sprite_index;
        double col_prec = 0.0125;
        std::vector<int> collides_with = {1};
        bounding_box collision_box = { 0, 0, 32, 40 };
	public:
        player() 
        {
            x = geptr->GetCanvasW() / 2;
            y = geptr->GetCanvasH() / 2;
            sprite_index = geptr->AddSprite("wario.png");
        };
		~player() {};
		void begin_step() {};
		void step() 
        {
            // Calculate velocities
            if (!check_collision_solid(x, y + col_prec))
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
            double accel = check_collision_solid(x, y + col_prec) ? g_accel : a_accel;
            double deccel = check_collision_solid(x, y + col_prec) ? g_deccel : a_deccel;

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

            // Jumping logic
            if (!is_true_a)
                yvel = std::max(yvel, minjump);
            if ((check_collision_solid(floor(x), floor(y + 1)) || coyote_timer < coyote_time) && ((is_true_a && !was_true_a) || willjump))
            {
                yvel = jump;
                coyote_timer = coyote_time;
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

            // Apply the y velocity
            if (check_collision_solid(x + xv_t, y))
            {
                while (abs(xv_t) >= col_prec && !check_collision_solid(x + col_prec * (xv_t / abs(xv_t)), y))
                {
                    x += col_prec * (xv_t / abs(xv_t));
                    xv_t = (abs(xv_t) - col_prec) * (xv_t / abs(xv_t));
                }
                xv_t = 0;
                xvel = 0;
            }
            x += xv_t;

            // Apply the y velocity
            if (check_collision_solid(x, y + yv_t))
            {
                while (abs(yv_t) >= col_prec && !check_collision_solid(x, y + col_prec * (yv_t / abs(yv_t))))
                {
                    y += col_prec * (yv_t / abs(yv_t));
                    yv_t = (abs(yv_t) - col_prec) * (yv_t / abs(yv_t));
                }
                yv_t = 0;
                yvel = 0;
            }
            y += yv_t;


            // Wrap
            if (x < 0)
                x += geptr->GetCanvasW() * 2;
            if (y < 0)
                y += geptr->GetCanvasH() * 2;
            if (x >= geptr->GetCanvasW() * 2)
                x -= geptr->GetCanvasW() * 2;
            if (y >= geptr->GetCanvasH() * 2)
                y -= geptr->GetCanvasH() * 2;

            geptr->cam_offset_x = x * geptr->GetFontW() - geptr->GetScreenW() / 2;
            geptr->cam_offset_y = y * geptr->GetFontH() - geptr->GetScreenH() / 2;

            // Draw the character at the end
            geptr->DrawSprite(sprite_index, floor(x * geptr->GetFontW()-6), floor(y * geptr->GetFontH() - 22), 2, 2, geptr->entity);
            // geptr->DrawRect(floor(x * geptr->GetFontW() + collision_box.x), floor(y * geptr->GetFontH() + collision_box.y), collision_box.w, collision_box.h, { 255,0,0,255 }, geptr->entity);
        };
		void end_step() {};

        bool check_collision_solid(double x, double y)
        {
            int x1 = floor((x * geptr->GetFontW() + collision_box.x) / geptr->GetFontW());
            int y1 = floor((y * geptr->GetFontH() + collision_box.y) / geptr->GetFontH());
            int x2 = floor(((x * geptr->GetFontW() + collision_box.x) + collision_box.w-1) / geptr->GetFontW());
            int y2 = floor(((y * geptr->GetFontH() + collision_box.y) + collision_box.h-1) / geptr->GetFontH());

            for (int q = y1; q <= y2; q++)
            {
                for (int i = x1; i <= x2; i++)
                {
                    int xx = i;
                    int yy = q;
                    if (xx >= geptr->GetCanvasW() * 2)
                        xx -= geptr->GetCanvasW() * 2;
                    if (xx < 0)
                        xx += geptr->GetCanvasW() * 2;
                    if (yy >= geptr->GetCanvasH() * 2)
                        yy -= geptr->GetCanvasH() * 2;
                    if (yy < 0)
                        yy += geptr->GetCanvasH() * 2;

                    if (VectorContains<int>(collides_with, geptr->GetCollisionValue(xx, yy, geptr->stat)) ||
                        VectorContains<int>(collides_with, geptr->GetCollisionValue(xx, yy, geptr->dyn)))
                        return true;
                }
            }

            return false;
        }
};

// Master pre code
void GameInit()
{
    int q = geptr->GetCanvasH() - 1;
    for (int i = 0; i < geptr->GetCanvasW() * 2; i++)
    {
        geptr->DrawRect(i * geptr->GetFontW(), q * geptr->GetFontH(), geptr->GetFontW(), geptr->GetFontH(), { 255,0,0,255 }, geptr->background);
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
    was_true_up = is_true_up;
    is_true_up = geptr->GetKeyState(SDL_SCANCODE_UP);
    was_true_down = is_true_down;
    is_true_down = geptr->GetKeyState(SDL_SCANCODE_DOWN);

    float _x;
    float _y;
    geptr->GetMousePosition(&_x, &_y);

    _x += geptr->cam_offset_x / geptr->GetFontW();
    _y += geptr->cam_offset_y / geptr->GetFontH();

    if (_x >= geptr->GetCanvasW() * 2)
        _x -= geptr->GetCanvasW() * 2;
    if (_x < 0)
        _x += geptr->GetCanvasW() * 2;
    if (_y >= geptr->GetCanvasH() * 2)
        _y -= geptr->GetCanvasH() * 2;
    if (_y < 0)
        _y += geptr->GetCanvasH() * 2;

    _x = floor(_x);
    _y = floor(_y);

    if (geptr->GetMouseButtonState(SDL_BUTTON_LEFT))
    {
        geptr->DrawRect(_x * geptr->GetFontW(), _y * geptr->GetFontH(), geptr->GetFontW(), geptr->GetFontH(), { 255,0,0,255 }, geptr->background);
        geptr->SetCollisionValue(_x, _y, geptr->stat, 1);
    }
    if (geptr->GetMouseButtonState(SDL_BUTTON_RIGHT))
    {
        geptr->GetMousePosition(&_x, &_y);
        geptr->DrawRect(_x * geptr->GetFontW(), _y * geptr->GetFontH(), geptr->GetFontW(), geptr->GetFontH(), { 0,0,0,255 }, geptr->background);
        geptr->SetCollisionValue(_x, _y, geptr->stat, 0);
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