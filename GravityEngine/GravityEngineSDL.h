#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <windows.h>
#include <math.h>
#include <string>
        
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;

class GravityEngine_Core
{
    // Gravity Engine Types
    public:
        // Enum to define which graphical layer to work in
        enum layer
        {
            ui, foreground, background, entity, debug
        };
        // Enum to define which collision layer to work in
        enum col_layer
        {
            stat, dyn
        };

    // Gravity Engine Private Attributes
    private: 
        bool game_running = false; // Is the game running or now?
        int canvas_w; // Game canvas width 
        int canvas_h; // Game canvas height
        char** canvas_debug; // Game canvas UI layer
        char** canvas_ui; // Game canvas UI layer
        char** canvas_fg; // Game canvas foreground layer
        char** canvas_bg; // Game canvas background layer
        char** canvas_ent; // Game canvas entity layerd
        short** color_debug; // Game color UI layer
        short** color_ui; // Game color UI layer
        short** color_fg; // Game color foreground layer
        short** color_bg; // Game color background layer
        short** color_ent; // Game color entity layer*/
        int** collision_static; // Game static collision layer
        int** collision_dynamic; // Game dynamic collision layer
        CHAR_INFO* buf_screen; // The final game canvas
        int elapsed_frames = 0; // Frames since game was started
        int frame_step_precision = 10000000; // Precision of sleep time to ensure the game stays in sync
        char def_char = ' '; // Default character to clean the garphics arrays
        short def_color = 7; // Default color to clean the color arrays
        int def_col = 0; // Default collision value to clean the collision arrays with
        int font_w; // Width of the font
        int font_h; // Height of the font
        int64_t frame_time = 0; // The current time the last frame took
        int64_t frame_length; // The desired frame length
        std::chrono::system_clock::time_point gobal_start_time; // When the game started
        std::chrono::system_clock::time_point start_time = std::chrono::system_clock::now(); // Beginning of the frame
        std::chrono::system_clock::time_point end_time = std::chrono::system_clock::now(); // End of the frame
        int current_fps = 0;
        const char* game_title;
        const char* game_id;
        const char* game_version;

    // Gravity Engine Public Attributes
    public: 
        bool debug_mode = false; // Show debug overlay
        bool debug_complex = false; // Show complex debug overlay

    // Gravity Engine Public Methods
    public:

        // Gravity Engine Constructor
        // int cw : window width
        // int ch : window height
        // int fw : font width
        // int fh : font height
        // int f : frame rate cap in frames per second
        GravityEngine_Core(const char* gt, const char* gi, const char* gv, int cw, int ch, int fw, int fh, int f)
        {
            // Set the dims of the game canvas
            canvas_w = cw;
            canvas_h = ch;
            game_title = gt;
            game_id = gi;
            game_version = gv;

            // Set the dims of the font
            if (fw == -1)
                fw = (int)(floor(GetSystemMetrics(SM_CXSCREEN) / cw)/1.5);
            if (fh == -1)
                fh = (int)(floor(GetSystemMetrics(SM_CYSCREEN) / ch)/1.5);
            font_w = fw;
            font_h = fh;

            // Instantiate canvas debug
            canvas_debug = new char*[canvas_h];
            for (int i = 0; i < canvas_h; i++)
                canvas_debug[i] = new char[canvas_w];
            for (int i = 0; i < canvas_h; i++)
                for (int q = 0; q < canvas_w; q++)
                    canvas_debug[i][q] = def_char;
            // Instantiate canvas ui
            canvas_ui = new char*[canvas_h];
            for (int i = 0; i < canvas_h; i++)
                canvas_ui[i] = new char[canvas_w];
            for (int i = 0; i < canvas_h; i++)
                for (int q = 0; q < canvas_w; q++)
                    canvas_ui[i][q] = def_char;
            // Instantiate canvas fg
            canvas_fg = new char*[canvas_h];
            for (int i = 0; i < canvas_h; i++)
                canvas_fg[i] = new char[canvas_w];
            for (int i = 0; i < canvas_h; i++)
                for (int q = 0; q < canvas_w; q++)
                    canvas_fg[i][q] = def_char;
            // Instantiate canvas bg
            canvas_bg = new char*[canvas_h];
            for (int i = 0; i < canvas_h; i++)
                canvas_bg[i] = new char[canvas_w];
            for (int i = 0; i < canvas_h; i++)
                for (int q = 0; q < canvas_w; q++)
                    canvas_bg[i][q] = def_char;
            // Instantiate canvas ent
            canvas_ent = new char*[canvas_h];
            for (int i = 0; i < canvas_h; i++)
                canvas_ent[i] = new char[canvas_w];
            for (int i = 0; i < canvas_h; i++)
                for (int q = 0; q < canvas_w; q++)
                    canvas_ent[i][q] = def_char;
            // Instantiate color debug
            color_debug = new short*[canvas_h];
            for (int i = 0; i < canvas_h; i++)
                color_debug[i] = new short[canvas_w];
            for (int i = 0; i < canvas_h; i++)
                for (int q = 0; q < canvas_w; q++)
                    color_debug[i][q] = def_color;
            // Instantiate color ui
            color_ui = new short*[canvas_h];
            for (int i = 0; i < canvas_h; i++)
                color_ui[i] = new short[canvas_w];
            for (int i = 0; i < canvas_h; i++)
                for (int q = 0; q < canvas_w; q++)
                    color_ui[i][q] = def_color;
            // Instantiate color fg
            color_fg = new short*[canvas_h];
            for (int i = 0; i < canvas_h; i++)
                color_fg[i] = new short[canvas_w];
            for (int i = 0; i < canvas_h; i++)
                for (int q = 0; q < canvas_w; q++)
                    color_fg[i][q] = def_color;
            // Instantiate color bg
            color_bg = new short*[canvas_h];
            for (int i = 0; i < canvas_h; i++)
                color_bg[i] = new short[canvas_w];
            for (int i = 0; i < canvas_h; i++)
                for (int q = 0; q < canvas_w; q++)
                    color_bg[i][q] = def_color;
            // Instantiate color ent
            color_ent = new short*[canvas_h];
            for (int i = 0; i < canvas_h; i++)
                color_ent[i] = new short[canvas_w];
            for (int i = 0; i < canvas_h; i++)
                for (int q = 0; q < canvas_w; q++)
                    color_ent[i][q] = def_color;
            // Instantiate collision static
            collision_static = new int*[canvas_h];
            for (int i = 0; i < canvas_h; i++)
                collision_static[i] = new int[canvas_w];
            for (int i = 0; i < canvas_h; i++)
                for (int q = 0; q < canvas_w; q++)
                    collision_static[i][q] = def_col;
            // Instantiate collision dynamic
            collision_dynamic = new int*[canvas_h];
            for (int i = 0; i < canvas_h; i++)
                collision_dynamic[i] = new int[canvas_w];
            for (int i = 0; i < canvas_h; i++)
                for (int q = 0; q < canvas_w; q++)
                    collision_dynamic[i][q] = def_col;

            // Instantiate screen buffer
            buf_screen = new CHAR_INFO[canvas_w * canvas_h];
            for (int i = 0; i < canvas_w * canvas_h; i++)
            {
                buf_screen[i].Char.UnicodeChar = ' ';
                buf_screen[i].Attributes = 7;
            }
            // Set the desired frame length to 1 second divided be the desired frame rate
            frame_length = 1000000000 / f;
        }
        
        // Gravity Engine Constructor
        // int cw : window width
        // int ch : window height
        // int f : frame rate cap in frames per second
        GravityEngine_Core(const char* gt, const char* gi, const char* gv, int cw, int ch, int f) : GravityEngine_Core(gt, gi, gv, cw, ch, -1, -1, f) {}
        
        // canvas x
        int GetCanvasW()
        {
            return canvas_w;
        }

        // canvas y
        int GetCanvasH()
        {
            return canvas_h;
        }
        
        // get collision value
        int GetCollisionValue(int x, int y, col_layer cl)
        {
            if (cl == stat)
                return collision_static[y][x];
            else if (cl == dyn)
                return collision_dynamic[y][x];
        } 

        // set collision value
        void SetCollisionValue(int x, int y, col_layer cl, int v)
        {
            if (cl == stat)
                collision_static[y][x] = v;
            else if (cl == dyn)
                collision_dynamic[y][x] = v;
        } 

        // Gravity Engine Destructor
        ~GravityEngine_Core()
        {
            // Delete all of the layers
            for (int i = 0; i < canvas_h; i++)
                delete[] canvas_debug[i];
            delete[] canvas_debug;
            for (int i = 0; i < canvas_h; i++)
                delete[] canvas_ui[i];
            delete[] canvas_ui;
            for (int i = 0; i < canvas_h; i++)
                delete[] canvas_fg[i];
            delete[] canvas_fg;
            for (int i = 0; i < canvas_h; i++)
                delete[] canvas_bg[i];
            delete[] canvas_bg;
            for (int i = 0; i < canvas_h; i++)
                delete[] canvas_ent[i];
            delete[] canvas_ent;
            for (int i = 0; i < canvas_h; i++)
                delete[] color_ui[i];
            delete[] color_ui;
            for (int i = 0; i < canvas_h; i++)
                delete[] color_fg[i];
            delete[] color_fg;
            for (int i = 0; i < canvas_h; i++)
                delete[] color_bg[i];
            delete[] color_bg;
            for (int i = 0; i < canvas_h; i++)
                delete[] color_ent[i];
            delete[] color_ent;
            for (int i = 0; i < canvas_h; i++)
                delete[] color_debug[i];
            delete[] color_debug;
            for (int i = 0; i < canvas_h; i++)
                delete[] collision_static[i];
            delete[] collision_static;
            for (int i = 0; i < canvas_h; i++)
                delete[] collision_dynamic[i];
            delete[] collision_dynamic;
        }

        // Start the video game
        SDL_AppResult Start(void (*init_game)() = nullptr, void (*pre_loop_code)() = nullptr, void (*post_loop_code)() = nullptr)
        {
            // Game is running now
            game_running = true;

            // Initialize SDL app meta data
            SDL_SetAppMetadata(game_title, game_version, game_id);
            // Initialize SDL library
            SDL_Init(SDL_INIT_VIDEO);
            // Create the SDL window
            SDL_CreateWindowAndRenderer(game_title, canvas_w * font_w, canvas_h * font_h, 0, &window, &renderer);

            // Init SDL ttf
            TTF_Init();

            // Call init custom user code
            if (init_game != nullptr)
                init_game();

            // Call game loop
            GameLoop(pre_loop_code, post_loop_code);

            // TTF Quit
            TTF_Quit();

            // Kill SDL
            SDL_Quit();

            // Success!
            return SDL_APP_SUCCESS;
        }

        // End game loop
        void End()
        {
            game_running = false;
        }

        // Delta time syncing function
        double DeltaTime()
        {
            // Return the delta time. If the frame time is equal to the ideal frame length, then this will be zero
            double t = ((frame_time * 1.0) / (frame_length  * 1.0));
            return t;
        }

        // Get current fps
        int fps_now()
        {
            return current_fps;
        }

        // Set the color of a layer pixel
        // int x : x position to place color
        // int y : y position to place color
        // int color : color value 0 - 137
        void DrawSetColor(int x, int y, layer l, int color)
        {
            // If the requested x and y is within the window...
            if (x >= 0 && x < canvas_w && y >= 0 && y < canvas_h)
            {
                // Set the color of the layer at these coordinates.
                if (l == debug) color_debug[y][x] = color;
                if (l == ui) color_ui[y][x] = color;
                if (l == background) color_bg[y][x] = color;
                if (l == foreground) color_fg[y][x] = color;
                if (l == entity) color_ent[y][x] = color;
            }
        }

        // Draw character onto layer
        void DrawChar(int x, int y, layer l, char c)
        {
            // If the requested x and y is within the window...
            if (x >= 0 && x < canvas_w && y >= 0 && y < canvas_h)
            {
                // Set the character at these coordinates on the layer.
                if (l == debug) canvas_debug[y][x] = c;
                if (l == ui) canvas_ui[y][x] = c;
                if (l == background) canvas_bg[y][x] = c;
                if (l == foreground) canvas_fg[y][x] = c;
                if (l == entity) canvas_ent[y][x] = c;
            }
        }

        // Draw text onto layer
        void DrawTextString(int x, int y, layer l, std::string s, short col)
        {
            if (l == background)
            {
                for (int q = 0; q < s.length(); q++)
                {
                    if (q+x < canvas_w)
                    {
                        canvas_bg[y][q+x] = s.at(q);
                        color_bg[y][q+x] = col;
                    }
                }
            }
            if (l == foreground)
            {
                for (int q = 0; q < s.length(); q++)
                {
                    if (q+x < canvas_w)
                    {
                        canvas_fg[y][q+x] = s.at(q);
                        color_fg[y][q+x] = col;
                    }
                }
            }
            if (l == ui)
            {
                for (int q = 0; q < s.length(); q++)
                {
                    if (q+x < canvas_w)
                    {
                        char c = s.at(q);
                        canvas_ui[y][q+x] = c;
                        color_ui[y][q+x] = col;
                    }
                }
            }
            if (l == entity)
            {
                for (int q = 0; q < s.length(); q++)
                {
                    if (q+x < canvas_w)
                    {
                        canvas_ent[y][q+x] = s.at(q);
                        color_ent[y][q+x] = col;
                    }
                }
            }
            if (l == debug)
            {
                for (int q = 0; q < s.length(); q++)
                {
                    if (q+x < canvas_w)
                    {
                        canvas_debug[y][q+x] = s.at(q);
                        color_debug[y][q+x] = col;
                    }
                }
            }
        }

        // Credit to OLC Console Game Engine for this function
        void Draw(int x, int y, short c = 0x2588, short col = 0x000F)
        {
            if (x >= 0 && x < canvas_w && y >= 0 && y < canvas_h)
            {
                if (buf_screen[y * canvas_w + x].Char.UnicodeChar != c)
                    buf_screen[y * canvas_w + x].Char.UnicodeChar = c;
                if (buf_screen[y * canvas_w + x].Attributes != col)
                    buf_screen[y * canvas_w + x].Attributes = col;
            }
        }

        // Get elapsed_frames
        long GetElapsedFrames()
        {
            return elapsed_frames;
        }

        // Get Key State
        bool GetKey(int key_code)
        {
            return (GetKeyState(key_code) & 0x8000);
        }

    private:

        // Pre-game code
        void SystemPreGameLoop()
        {
        }

        // Mid-game code
        void SystemGameLoop()
        {
            // Draw the background layer
            DrawLayers();

            // Poll SDL
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                // Get close event
                if (event.type == SDL_EVENT_QUIT)
                    game_running = false;
            }
        }

        // Draw the console buffer
        void DrawConsoleBuffer()
        {
            for (int i = 0; i < canvas_h; i++)
            {
                for (int q = 0; q < canvas_w; q++)
                {
                    char thisChar = buf_screen[i * canvas_w + q].Char.UnicodeChar;
                    
                }
            }
        }

        // Post-game code
        void SystemPostGameLoop()
        {
            // Frame count
            elapsed_frames++;

            // Draw to the window

            // Clear the Dynamic Collision, Entity, and Debug layers
            for (int i = 0; i < canvas_h; i++)
            {
                for (int q = 0; q < canvas_w; q++)
                {
                    DrawChar(q, i, entity, ' ');
                    DrawChar(q, i, debug, ' ');
                    SetCollisionValue(q, i, dyn, 0);
                }
            }
        }

        // Set console color
        void SetColor(int color)
        {
            HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);   
            SetConsoleTextAttribute(output, color);
        }

        // Draw all the layers onto the console buffer. Do not write if it is blank or the layer above is obfuscating this coord
        void DrawLayers()
        {
            for (int i = 0; i < canvas_h; i++)
            {
                for (int q = 0; q < canvas_w; q++)
                {
                    if (canvas_fg[i][q]==' ' && canvas_debug[i][q]==' ' && canvas_ent[i][q]==' ' && canvas_ui[i][q]==' ')
                        Draw(q,i,canvas_bg[i][q],color_bg[i][q]);
                    if (canvas_ent[i][q] != ' ')
                        if (canvas_fg[i][q]==' ' && canvas_debug[i][q]==' ' && canvas_ui[i][q]==' ')
                            Draw(q,i,canvas_ent[i][q],color_ent[i][q]);
                    if (canvas_fg[i][q] != ' ')
                        if (canvas_debug[i][q]==' ' && canvas_ui[i][q]==' ')
                            Draw(q,i,canvas_fg[i][q],color_fg[i][q]);
                    if (canvas_ui[i][q] != ' ')
                        if (canvas_debug[i][q]==' ')
                            Draw(q,i,canvas_ui[i][q],color_ui[i][q]);
                    if (canvas_debug[i][q] != ' ')
                        Draw(q,i,canvas_debug[i][q],color_debug[i][q]);
                }
            }
        }

        // Log timing
        void LogFrameTimingData(long* frame_check, long* second_check, long* frames_per_second)
        {
            // Log timing  
            SetColor(7);
            (*frame_check)++;
            double seconds = std::chrono::duration<double, std::milli>(std::chrono::system_clock::now() - gobal_start_time).count() / 1000;
            if (seconds > (*second_check))
            {
                (*frames_per_second) = (*frame_check);
                (*frame_check) = 0;
                (*second_check) += 1;
            }
            
            // Draw the debug overlay
            if (debug_complex)
            {
                DrawTextString(0,0,debug,"Delta Time: " + std::to_string(DeltaTime()) + " Elapsed Seconds: " + std::to_string(seconds),7);
                DrawTextString(0,1,debug,"Frame Time: " + std::to_string(frame_time) + " FPS: " + std::to_string(*frames_per_second),7);
            }
            else
            {
                DrawTextString(0,0,debug,"FPS: " + std::to_string(*frames_per_second),7);
            }

            // Set the fpd variable
            current_fps = *frames_per_second;
        }

        // Sync frame step
        void SyncFrameStep(int frame_step_precision)
        {
            // Sync timing
            std::chrono::duration<int64_t, std::nano> delta(frame_length);
            auto next_frame = start_time + delta;
            std::chrono::duration<int64_t, std::nano> frame_delay(next_frame - std::chrono::system_clock::now());
            while (std::chrono::system_clock::now() < next_frame) { /* Spin in place until the clock hits the next frame */ }
            // Get the end of the frame time
            end_time = std::chrono::system_clock::now();
            frame_time = (end_time - start_time).count();
        }

        // Game loop
        void GameLoop(void (*pre_loop_code)(), void (*post_loop_code)())
        {            
            // Init timing stuff
            gobal_start_time = std::chrono::system_clock::now();
            long second_check = 1;
            long frames_per_second = 0;
            long frame_check;

            while (game_running)
            {
                // Pre-timing
                start_time = std::chrono::system_clock::now();

                // Run pre-loop engine code
                SystemPreGameLoop();

                // Call pre custom user code
                if (pre_loop_code != nullptr)
                    pre_loop_code();

                // Run engine code
                SystemGameLoop();

                // Call post custom user code
                if (post_loop_code != nullptr)
                    post_loop_code();
                    
                // Run post-loop engine code
                SystemPostGameLoop();

                // Log frame timing to console
                if (debug_mode)
                    LogFrameTimingData(&frame_check, &second_check, &frames_per_second);

                // Ensure frame-rate stays within requested FPS
                SyncFrameStep(frame_step_precision);
            }
        }
};