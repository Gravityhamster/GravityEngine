#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <math.h>
#include <vector>
#include <string>
#include <unordered_map>

// Copilot help on this one
std::vector<Uint8> ConvertAudio(Uint8* audio_buf, Uint32 audio_len, SDL_AudioSpec wav_audio_spec, SDL_AudioSpec audio_spec)
{
    // Convert audio to spec
    auto sdl_audio_stream_conv = SDL_CreateAudioStream(&wav_audio_spec, &audio_spec);
    SDL_PutAudioStreamData(sdl_audio_stream_conv, audio_buf, audio_len);
    SDL_FlushAudioStream(sdl_audio_stream_conv);
    std::vector<Uint8> converted_data;
    Uint8 temp[4096];
    int bytesRead;
    while ((bytesRead = SDL_GetAudioStreamData(sdl_audio_stream_conv, temp, sizeof(temp))) > 0) {
        converted_data.insert(converted_data.end(), temp, temp + bytesRead);
    }
    SDL_DestroyAudioStream(sdl_audio_stream_conv);
    return converted_data;
}

// Template for game objects
class GravityEngine_Object
{
    public:
        GravityEngine_Object() {};
		virtual ~GravityEngine_Object() {};
		virtual void begin_step() {};
		virtual void step() {};
		virtual void end_step() {};
};

// Core engine class
class GravityEngine_Core
{
    // Gravity Engine classes
    private:
        // Gravity Engine channel state
        enum ChannelStates
        {
            uninit,
            init,
            playing,
            paused,
            stopped
        };

        // Gravity Engine sound class
        class GravityEngine_Sound
        {
        public:
            // -= Attributes =-
            Uint8* audio_buf;
            Uint32 audio_len;
            SDL_AudioSpec wav_audio_spec;
            std::vector<Uint8> converted_audio;

            // -= Methods =-

            // Construct audio
            GravityEngine_Sound(const char* path, SDL_AudioSpec audio_spec)
            {
                // Load the wav file
                SDL_LoadWAV(path, &wav_audio_spec, &audio_buf, &audio_len);
                // Convert the audio
                converted_audio = ConvertAudio(audio_buf, audio_len, wav_audio_spec, audio_spec);
            };

            // Destruct audio
            ~GravityEngine_Sound() {};
        };

        // GravityEngine audio channel class
        class GravityEngine_AudioChannel
        {
        private:
            // -= Attributes =-
            SDL_AudioStream* sdl_audio_stream = nullptr;
            SDL_AudioDeviceID audio_device_id;
            ChannelStates state = uninit;

        public:
            // -= Methods =-

            // Construct audio
            GravityEngine_AudioChannel(SDL_AudioSpec audio_spec)
            {
                // Create the audio stream
                sdl_audio_stream = SDL_CreateAudioStream(&audio_spec, &audio_spec);
                audio_device_id = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &audio_spec);
                state = init;
            };

            // Play a sound on this channel
            void PlaySound(SDL_AudioSpec audio_spec, GravityEngine_Sound* gravity_engine_sound_ref)
            {
                StopPlayback();
                SDL_PutAudioStreamData(sdl_audio_stream, gravity_engine_sound_ref->converted_audio.data(), gravity_engine_sound_ref->converted_audio.size());
                SDL_BindAudioStream(audio_device_id, sdl_audio_stream);
                SDL_ResumeAudioDevice(audio_device_id);
                state = playing;
            }

            // Stop audio
            void StopPlayback()
            {
                SDL_ClearAudioStream(sdl_audio_stream);
                state = stopped;
            }

            // Pause audio
            void PausePlayback()
            {
                SDL_PauseAudioDevice(audio_device_id);
                state = paused;
            }

            // Continue audio
            void ResumePlayback()
            {
                SDL_ResumeAudioDevice(audio_device_id);
                state = playing;
            }

            // Get state of channel
            ChannelStates GetState()
            {
                return state;
            }

            // Destruct audio
            ~GravityEngine_AudioChannel()
            {
                SDL_CloseAudioDevice(audio_device_id);
            };
        };

    // Gravity Engine Types
    public:
        const struct SDL_AudioSpec global_audio_spec = { SDL_AUDIO_S32LE,2,48000 }; // Manual spec
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
        // Color struct (foreground and background)
        struct color
        {
        	SDL_Color f;
        	SDL_Color b;
        };

    // Gravity Engine Private Attributes
    private:
    	std::vector<GravityEngine_Object*> entity_list;
        bool game_running = false; // Is the game running or now?
        int canvas_w; // Game canvas width
        int canvas_h; // Game canvas height
        char** canvas_debug; // Game canvas UI layer
        char** canvas_ui; // Game canvas UI layer
        char** canvas_fg; // Game canvas foreground layer
        char** canvas_bg; // Game canvas background layer
        char** canvas_ent; // Game canvas entity layers
        color** color_debug; // Game color UI layer
        color** color_ui; // Game color UI layer
        color** color_fg; // Game color foreground layer
        color** color_bg; // Game color background layer
        color** color_ent; // Game color entity layer*/
        char** collision_static; // Game static collision layer
        char** collision_dynamic; // Game dynamic collision layer
        char* buf_char_screen; // The final game canvas
        color* buf_col_screen; // The final color canvas
        char* last_buf_char_screen; // The final game canvas
        color* last_buf_col_screen; // The final color canvas
        int elapsed_frames = 0; // Frames since game was started
        int frame_step_precision = 10000000; // Precision of sleep time to ensure the game stays in sync
        char def_char = ' '; // Default character to clean the graphics arrays
        color def_color = {{255,255,255}, {0,0,0}}; // Default color to clean the color arrays
        int def_col = 0; // Default collision value to clean the collision arrays with
        int font_w; // Width of the font
        int font_disp_x = 0; // Horizontal font displacement
        int font_h; // Height of the font
        int64_t frame_time = 0; // The current time the last frame took
        int64_t frame_length; // The desired frame length
        std::chrono::system_clock::time_point gobal_start_time; // When the game started
        std::chrono::system_clock::time_point start_time = std::chrono::system_clock::now(); // Beginning of the frame
        std::chrono::system_clock::time_point end_time = std::chrono::system_clock::now(); // End of the frame
        int current_fps = 0; // The games current frame rate
        const char* game_title; // The name of the game
        const char* game_id; // The game's id
        const char* game_version; // The version of the game
        int scr_w; // W of screen
        int scr_h; // H of screen
        int SDL_window_props = SDL_WINDOW_FULLSCREEN; //0;
        bool screen_updated = false; // The flag that tells the game if it should update the screen or not
        std::string font_path; // Location of the font to use for the text on screen
        SDL_Window* window = NULL; // Pointer to the SDL window object
        SDL_Renderer* renderer = NULL; // Pointer to the SDL renderer object
        SDL_Texture* render_texture = NULL; // The texture for the entire screen
        SDL_Texture* char_texture = NULL; // The texture for the current character to draw
        TTF_TextEngine* engine = NULL; // Point to the SDL_ttf text engine (only used if glyph_precaching is off)
        TTF_Font* sans = NULL; // SDL_ttf font to use
        TTF_Text **draw_chars = NULL; // List of character objects that are drawn in a grid (only used if glyph_precaching is off)
        std::unordered_map<std::string, SDL_Texture*> character_textures = {}; // Texture cache (only used if glyph_precaching is on)
        std::unordered_map<std::string, int> character_widths = {}; // Texture width cache (only used if glyph_precaching is on)
        const bool* keyboard_keys = SDL_GetKeyboardState(NULL); // Initialize keystate list
        std::vector<GravityEngine_AudioChannel*> audio_channels;
        std::vector<GravityEngine_Sound*> sounds;
        int channels;

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
        GravityEngine_Core(const char* gt, const char* gi, const char* gv, int cw, int ch, int fw, int fh, int f, int sw, int sh, std::string fp, int c)
        {
        	// Set game font
        	font_path = fp;

        	// Set screen resolution
        	scr_w = sw;
        	scr_h = sh;

            // Set the dims of the game canvas
            canvas_w = cw;
            canvas_h = ch;
            game_title = gt;
            game_id = gi;
            game_version = gv;

            // Set the dims of the font
            if (fw == -1)
                fw = (int)(floor(scr_w / cw));
            if (fh == -1)
                fh = (int)(floor(scr_h / ch));
            font_w = fw;
            font_h = fh;
            font_disp_x = font_w;

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
            color_debug = new color*[canvas_h];
            for (int i = 0; i < canvas_h; i++)
                color_debug[i] = new color[canvas_w];
            for (int i = 0; i < canvas_h; i++)
                for (int q = 0; q < canvas_w; q++)
                    color_debug[i][q] = def_color;
            // Instantiate color ui
            color_ui = new color*[canvas_h];
            for (int i = 0; i < canvas_h; i++)
                color_ui[i] = new color[canvas_w];
            for (int i = 0; i < canvas_h; i++)
                for (int q = 0; q < canvas_w; q++)
                    color_ui[i][q] = def_color;
            // Instantiate color fg
            color_fg = new color*[canvas_h];
            for (int i = 0; i < canvas_h; i++)
                color_fg[i] = new color[canvas_w];
            for (int i = 0; i < canvas_h; i++)
                for (int q = 0; q < canvas_w; q++)
                    color_fg[i][q] = def_color;
            // Instantiate color bg
            color_bg = new color*[canvas_h];
            for (int i = 0; i < canvas_h; i++)
                color_bg[i] = new color[canvas_w];
            for (int i = 0; i < canvas_h; i++)
                for (int q = 0; q < canvas_w; q++)
                    color_bg[i][q] = def_color;
            // Instantiate color ent
            color_ent = new color*[canvas_h];
            for (int i = 0; i < canvas_h; i++)
                color_ent[i] = new color[canvas_w];
            for (int i = 0; i < canvas_h; i++)
                for (int q = 0; q < canvas_w; q++)
                    color_ent[i][q] = def_color;
            // Instantiate collision static
            collision_static = new char*[canvas_h];
            for (int i = 0; i < canvas_h; i++)
                collision_static[i] = new char[canvas_w];
            for (int i = 0; i < canvas_h; i++)
                for (int q = 0; q < canvas_w; q++)
                    collision_static[i][q] = def_col;
            // Instantiate collision dynamic
            collision_dynamic = new char*[canvas_h];
            for (int i = 0; i < canvas_h; i++)
                collision_dynamic[i] = new char[canvas_w];
            for (int i = 0; i < canvas_h; i++)
                for (int q = 0; q < canvas_w; q++)
                    collision_dynamic[i][q] = def_col;

            // Instantiate screen buffer
            buf_char_screen = new char[canvas_w * canvas_h];
            buf_col_screen = new color[canvas_w * canvas_h];
            last_buf_char_screen = new char[canvas_w * canvas_h];
            last_buf_col_screen = new color[canvas_w * canvas_h];
            for (int i = 0; i < canvas_w * canvas_h; i++)
            {
                buf_char_screen[i] = ' ';
                buf_col_screen[i] = {{255,255,255},{0,0,0}};
                last_buf_char_screen[i] = '1';
                last_buf_col_screen[i] = { {0,0,0},{0,0,0} };
            }
            // Set the desired frame length to 1 second divided be the desired frame rate
            frame_length = 1000000000 / f;
            // Set channel count
            channels = c;
        }

        // Gravity Engine Constructor
        // int cw : window width
        // int ch : window height
        // int f : frame rate cap in frames per second
        GravityEngine_Core(const char* gt, const char* gi, const char* gv, int cw, int ch, int f, int w, int h, std::string fp, int c) : GravityEngine_Core(gt, gi, gv, cw, ch, -1, -1, f, w, h, fp, c) {}

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
            else
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
            delete[] canvas_debug;
            delete[] canvas_ui;
            delete[] canvas_fg;
            delete[] canvas_bg;
            delete[] canvas_ent;
            delete[] color_ui;
            delete[] color_fg;
            delete[] color_bg;
            delete[] color_ent;
            delete[] color_debug;
            delete[] collision_static;
            delete[] collision_dynamic;
            delete[] buf_col_screen;
            delete[] buf_char_screen;
        }

        // Start the video game
        SDL_AppResult Start(void (*init_game)() = nullptr, void (*pre_loop_code)() = nullptr, void (*post_loop_code)() = nullptr)
        {
            // Game is running now
            game_running = true;

            // Initialize SDL app meta data
            SDL_SetAppMetadata(game_title, game_version, game_id);
            // Initialize SDL library
            if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) == false)
            {
                std::cout << SDL_GetError() << std::endl;
                std::system("pause");
            }
            // Create the SDL window
            SDL_CreateWindowAndRenderer(game_title, canvas_w * font_w, canvas_h * font_h, SDL_window_props, &window, &renderer);

            // Initialize all audio channels
            for (int i = 0; i < channels; i++)
                audio_channels.insert(audio_channels.end(), new GravityEngine_AudioChannel(global_audio_spec));

            // Create the render texture
            render_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, canvas_w * font_w, canvas_h * font_h);
            char_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, font_w, font_h);

            // Create the engine used to write text
            engine = TTF_CreateRendererTextEngine(renderer);

            // Init SDL ttf
            TTF_Init();

            // Create font
            const char * fp = font_path.c_str();
            sans = TTF_OpenFont(fp, font_h);

            // Create texts
            draw_chars = new TTF_Text*[canvas_w * canvas_h];
            for (int i = 0; i < canvas_w * canvas_h; i++)
                draw_chars[i] = TTF_CreateText(engine, sans, "0", 0u);

            // Call init custom user code
            if (init_game != nullptr)
                init_game();

            // Call game loop
            GameLoop(pre_loop_code, post_loop_code);

            // TTF Quit
			for (int i = 0; i < canvas_w * canvas_h; i++)
				TTF_DestroyText(draw_chars[i]);
			delete[] draw_chars;
			TTF_DestroyRendererTextEngine(engine);
            TTF_Quit();

            // Kill SDL
            SDL_Quit();

            // Free audio channels
            for (auto ac : audio_channels)
                delete ac;
            for (auto s : sounds)
                delete s;

            // Free all objects
        	for (auto o : entity_list)
        		delete o;

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
        void DrawSetColor(int x, int y, layer l, color col)
        {
            // If the requested x and y is within the window...
            if (x >= 0 && x < canvas_w && y >= 0 && y < canvas_h)
            {
                // Set the color of the layer at these coordinates.
                if (l == debug) color_debug[y][x] = col;
                if (l == ui) color_ui[y][x] = col;
                if (l == background) color_bg[y][x] = col;
                if (l == foreground) color_fg[y][x] = col;
                if (l == entity) color_ent[y][x] = col;
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

        // Change Font
        void ChangeFont(std::string fpth)
        {
            // Reset the last buffer so the entire frame gets drawn
            for (int i = 0; i < canvas_w * canvas_h; i++)
            {
                last_buf_char_screen[i] = NULL;
                last_buf_col_screen[i] = { {NULL,NULL,NULL},{NULL,NULL,NULL} };
            }
            // Create font
            const char * fp = fpth.c_str();
            sans = TTF_OpenFont(fp, font_h);
        }

        // Draw text onto layer
        void DrawTextString(int x, int y, layer l, std::string s, color col)
        {
        	// Draw text to the background layer
            if (l == background)
            {
            	// Loop through the string and drop the characters into the canvas
            	int q = 0;
                for (auto c : s)
                {
                    if (q+x < canvas_w)
                    {
                        canvas_bg[y][q+x] = c;
                        color_bg[y][q+x] = col;
                    }
                    q++;
                }
            }
        	// Draw text to the foreground layer
            else if (l == foreground)
            {
            	// Loop through the string and drop the characters into the canvas
            	int q = 0;
                for (auto c : s)
                {
                    if (q+x < canvas_w)
                    {
                        canvas_fg[y][q+x] = c;
                        color_fg[y][q+x] = col;
                    }
                    q++;
                }
            }
        	// Draw text to the ui layer
            else if (l == ui)
            {
            	// Loop through the string and drop the characters into the canvas
            	int q = 0;
                for (auto c : s)
                {
                    if (q+x < canvas_w)
                    {
                        canvas_ui[y][q+x] = c;
                        color_ui[y][q+x] = col;
                    }
                    q++;
                }
            }
        	// Draw text to the entity layer
            else if (l == entity)
            {
            	// Loop through the string and drop the characters into the canvas
            	int q = 0;
                for (auto c : s)
                {
                    if (q+x < canvas_w)
                    {
                        canvas_ent[y][q+x] = c;
                        color_ent[y][q+x] = col;
                    }
                    q++;
                }
            }
        	// Draw text to the debug layer
            else if (l == debug)
            {
            	// Loop through the string and drop the characters into the canvas
            	int q = 0;
                for (auto c : s)
                {
                    if (q+x < canvas_w)
                    {
                        canvas_debug[y][q+x] = c;
                        color_debug[y][q+x] = col;
                    }
                    q++;
                }
            }
        }

        // Get elapsed_frames
        long GetElapsedFrames()
        {
            return elapsed_frames;
        }

        // Handle input
        bool GetKeyState(SDL_Keymod sdlKey)
        {
            SDL_PumpEvents();
            if (keyboard_keys[sdlKey])
                return true;
            else
                return false;
        }

        // Add the object to the entity list
        void AddObject(GravityEngine_Object* object)
        {
        	entity_list.push_back(object);
        }

        // Add sounds to the sound list
        int AddSound(const char* path)
        {
            // Initialize all audio channels
            sounds.insert(sounds.end(), new GravityEngine_Sound(path, global_audio_spec));
            return sounds.size() - 1;
        }

        // Delete sound from the sound list
        void DeleteSound(int index)
        {
            // Delete the sound objects
            delete[] sounds[index];
            // Set this index to a nullptr
            sounds[index] = nullptr;
        }

        // Play a sound on a channel
        void PlaySoundOnChannel(int audio_index, int channel)
        {
            channel = channel % audio_channels.size();
            audio_channels[channel]->PlaySound(global_audio_spec, sounds[audio_index]);
        }

        // Pause channel
        void PauseChannel(int channel)
        {
            audio_channels[channel]->PausePlayback();
        }

        // Resume channel
        void ResumeChannel(int channel)
        {
            audio_channels[channel]->ResumePlayback();
        }

        // Stop channel
        void StopChannel(int channel)
        {
            audio_channels[channel]->StopPlayback();
        }

    private:

        // Draw character into the screen buffer with char c and color col
        void Draw(int x, int y, char c = '0', color col = {{255, 255, 255}, {0, 0, 0}})
        {
        	// Only write if the character is in the bounds of the screen buffer
            if (x >= 0 && x < canvas_w && y >= 0 && y < canvas_h)
            {
                buf_char_screen[y * canvas_w + x] = c;
                buf_col_screen[y * canvas_w + x] = col;
            }
        }
        
        // Draw screen buffer to the SDL window
        void DrawScreenText()
        {
        	// Init the iterator for the character buffer
        	int buf_index = 0;
        	// Init the x and y coords for the output
        	int x = 0;
        	int y = 0;
            // Render to texture instead of directly to the screen
            SDL_SetRenderTarget(renderer, render_texture);
        	// Loop through all of the characters in the
            for (int i = 0; i < canvas_w * canvas_h; i++)
            {
            	// Newline
            	if (i % canvas_w == 0 && i != 0)
            	{
            		y++;
            		x = 0;
            	}

            	// Check drawing mode
                if (buf_char_screen[buf_index] != last_buf_char_screen[buf_index] ||
                    buf_col_screen[buf_index].b.r != last_buf_col_screen[buf_index].b.r ||
                    buf_col_screen[buf_index].b.g != last_buf_col_screen[buf_index].b.g ||
                    buf_col_screen[buf_index].b.b != last_buf_col_screen[buf_index].b.b ||
                    buf_col_screen[buf_index].f.r != last_buf_col_screen[buf_index].f.r ||
                    buf_col_screen[buf_index].f.g != last_buf_col_screen[buf_index].f.g ||
                    buf_col_screen[buf_index].f.b != last_buf_col_screen[buf_index].f.b)
                {                    
                    // Please post the render at the end of the frame
                    screen_updated = true;
                    // Destroy the text object if it exists
                    if (draw_chars[buf_index] != NULL)
                        TTF_DestroyText(draw_chars[buf_index]);
                    // Create the text object for the character we are going to draw
                    draw_chars[buf_index] = TTF_CreateText(engine, sans, "0", 0u);
                    // Set the character's color
                    TTF_SetTextColor(draw_chars[buf_index],
                        (int)(buf_col_screen[buf_index].f.r),
                        (int)(buf_col_screen[buf_index].f.g),
                        (int)(buf_col_screen[buf_index].f.b),
                        255);
                    // Set the text's first character to the buffer's character at this location
                    draw_chars[buf_index]->text[0] = buf_char_screen[buf_index];
                    // Point to the texture for drawing a char
                    SDL_SetRenderTarget(renderer, char_texture);
                    // Draw glyph - Fill background with back color
                    SDL_SetRenderDrawColor(renderer,
                        buf_col_screen[buf_index].b.r,
                        buf_col_screen[buf_index].b.g,
                        buf_col_screen[buf_index].b.b,
                        255);
                    SDL_RenderFillRect(renderer, NULL);
                    // Render the text
                    TTF_DrawRendererText(draw_chars[buf_index], 0, 0);
                    // Go back to the previous and then draw the char texture
                    SDL_SetRenderTarget(renderer, render_texture);
                    // Define where the char will go in a rect
                    SDL_FRect c_rect;
                    c_rect.x = x * font_disp_x;
                    c_rect.y = y * font_h;
                    c_rect.w = font_w;
                    c_rect.h = font_h;
                    // Actually render the char texture to that rect
                    SDL_RenderTexture(renderer, char_texture, NULL, new SDL_FRect(c_rect));
                    // Clear the char texture
                    SDL_SetRenderTarget(renderer, char_texture);
                    SDL_RenderClear(renderer);
                }

                // Inc vars
            	x++;
            	buf_index++;
            }
            // Reset render back to screen
            SDL_SetRenderTarget(renderer, NULL);
        }
        
        // Pre-game code
        void SystemPreGameLoop()
        {
        	// Call all begin_step functions
        	for (auto o : entity_list)
        		(*o).begin_step();

            // Poll SDL
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                // Get close event
                if (event.type == SDL_EVENT_QUIT)
                    game_running = false;
            }
        }

        // Mid-game code
        void SystemGameLoop()
        {
        	// Call all step functions
        	for (auto o : entity_list)
        		(*o).step();

        	// Clear surface
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);

            // Draw the background layer
            DrawLayers();

            // Draw text
            DrawScreenText();
        }

        // Post-game code
        void SystemPostGameLoop()
        {
            // Frame count
            elapsed_frames++;

            // Draw to the window - Do not draw if the draw flag is off
            if (screen_updated)
            {
                // Draw the screen texture to the renderer
                SDL_RenderTexture(renderer, render_texture, NULL, NULL);
                // Present everything rendered to the renderer to the screen
                SDL_RenderPresent(renderer);
                // I dunno why I have this delay here
                SDL_Delay(0);
                // Reset the draw flag
                screen_updated = false;
            }

            // Copy current screen state into last screen state
            for (int i = 0; i < canvas_w * canvas_h; i++)
            {
                last_buf_char_screen[i] = buf_char_screen[i];
                last_buf_col_screen[i] = buf_col_screen[i];
            }

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

        	// Call all step functions
        	for (auto o : entity_list)
        		(*o).end_step();
        }

        // Draw all the layers onto the console buffer. Do not write if it is blank or the layer above is obfuscating this coord
        void DrawLayers()
        {
            for (int i = 0; i < canvas_h; i++)
            {
                for (int q = 0; q < canvas_w; q++)
                {
                	// Draw the debug layer only if debug mode is on
                    if (debug_mode)
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
                    else
                    {
						if (canvas_fg[i][q]==' ' && canvas_ent[i][q]==' ' && canvas_ui[i][q]==' ')
							Draw(q,i,canvas_bg[i][q],color_bg[i][q]);
						if (canvas_ent[i][q] != ' ')
							if (canvas_fg[i][q]==' ' && canvas_ui[i][q]==' ')
								Draw(q,i,canvas_ent[i][q],color_ent[i][q]);
						if (canvas_fg[i][q] != ' ')
							if (canvas_ui[i][q]==' ')
								Draw(q,i,canvas_fg[i][q],color_fg[i][q]);
						if (canvas_ui[i][q] != ' ')
								Draw(q,i,canvas_ui[i][q],color_ui[i][q]);
                    }
                }
            }
        }

        // Log timing
        void LogFrameTimingData(long* frame_check, long* second_check, long* frames_per_second)
        {
            // Log timing
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
                DrawTextString(0,0,debug,"Delta Time: " + std::to_string(DeltaTime()) + " Elapsed Seconds: " + std::to_string(seconds) + " Elapsed Frames: " + std::to_string(elapsed_frames),{{255,255,255},{0,0,0}});
                DrawTextString(0,1,debug,"Frame Time: " + std::to_string(frame_time) + " FPS: " + std::to_string(*frames_per_second),{{255,255,255},{0,0,0}});
            }
            else
            {
                DrawTextString(0,0,debug,"FPS: " + std::to_string(*frames_per_second),{{255,255,255},{0,0,0}});
            }

            // Set the fps variable
            current_fps = *frames_per_second;
        }

        // Sync frame step
        void SyncFrameStep(int frame_step_precision)
        {
            // Sync timing
            std::chrono::duration<int64_t, std::nano> delta(frame_length);
            auto next_frame = start_time + delta;
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