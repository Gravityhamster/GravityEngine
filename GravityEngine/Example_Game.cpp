#include "GravityEngineSDL.h"

GravityEngine_Core* geptr;
bool play_was_held = false;
bool play_is_held = false;
std::vector<m_node*> m_node_list;

// class example : public virtual GravityEngine_Object
// {
// private:
// public:
//     example() {};
//     ~example() {};
//     void begin_step() {};
//     void step() {};
//     void end_step() {};
// };

enum m_node_type
{
    origin,
    ping,
    left,
    right,
    up,
    down
};

class m_node : public virtual GravityEngine_Object
{
private:
    m_node_type type;
    int x;
    int y;
public:
    m_node(int xx, int yy, m_node_type t) { type = t; x = xx; y = yy; };
    ~m_node() {};
    void begin_step() {
        switch(type)
        {
            case origin:
                geptr->DrawChar(x, y, geptr->background, 'O');
                if (play_is_held && !play_was_held)
                {
                    // auto mn = new m_node(x, y, ping);
                    // geptr->AddObject(mn);
                }
                break;
            case up:
                geptr->DrawChar(x, y, geptr->background, '^');
                break;
            case down:
                geptr->DrawChar(x, y, geptr->background, 'V');
                break;
            case left:
                geptr->DrawChar(x, y, geptr->background, '<');
                break;
            case right:
                geptr->DrawChar(x, y, geptr->background, '>');
                break;
            case ping:
                geptr->DrawChar(x, y, geptr->foreground, '+');
                break;
        }
    };
    void step() {};
    void end_step() {};
};

int NewNode(int x, int y, m_node_type type)
{

}

// Master pre code
void GameInit()
{
    
}

// Master pre code
void PreGameLoop()
{
    // Mouse position
    int x, y;
    geptr->GetMousePosition(&x, &y); 
    auto c = geptr->GetChar(x, y, geptr->background);
    if (c == ' ')
    {
        geptr->DrawChar(x, y, geptr->entity, '_');
        geptr->DrawSetColor(x, y, geptr->entity, { {255, 255, 255}, {255, 255, 255} });
    }
    else
    {
        geptr->DrawChar(x, y, geptr->entity, c);
        geptr->DrawSetColor(x, y, geptr->entity, { {0, 0, 0}, {255, 255, 255} });
    }

    // Create musical mode
    if (geptr->GetKeyState(SDL_SCANCODE_O))
    {
        auto mn = new m_node(x, y, origin);
        geptr->AddObject(mn);
    }
    if (geptr->GetKeyState(SDL_SCANCODE_UP))
    {
        auto mn = new m_node(x, y, up);
        geptr->AddObject(mn);
    }
    if (geptr->GetKeyState(SDL_SCANCODE_DOWN))
    {
        auto mn = new m_node(x, y, down);
        geptr->AddObject(mn);
    }
    if (geptr->GetKeyState(SDL_SCANCODE_LEFT))
    {
        auto mn = new m_node(x, y, left);
        geptr->AddObject(mn);
    }
    if (geptr->GetKeyState(SDL_SCANCODE_RIGHT))
    {
        auto mn = new m_node(x, y, right);
        geptr->AddObject(mn);
    }

    // Get other inputs
    play_was_held = play_is_held;
    play_is_held = geptr->GetKeyState(SDL_SCANCODE_RETURN);
}

// Master post code
void PostGameLoop()
{
}

int main()
{
    // Init engine - 128x72 is generally the largest you can get and still maintain good performance
    GravityEngine_Core ge_inst = GravityEngine_Core("Game", "com.example.game", "1.0", 96/2, 54/2, 60, 1920, 1080, "./GameFont.ttf", 16);

    ge_inst.debug_mode = true; // Show debug overlay
    ge_inst.debug_complex = false; // Show all information
    geptr = &ge_inst; // Set the pointer to the console engine class

    // Start game loop
    ge_inst.Start(&GameInit, &PreGameLoop, &PostGameLoop);

    // Report success to host
    return 0;
}