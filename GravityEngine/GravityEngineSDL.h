#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <math.h>
#include <vector>
#include <string>
#include <unordered_map>

// Template for game objects
class GravityEngine_Object
{
    public:
		GravityEngine_Object() {}
		virtual ~GravityEngine_Object() {};
		virtual void begin_step() {};
		virtual void step() {};
		virtual void end_step() {};
};

// Core engine class
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
        // Color struct (foreground and background)
        struct color
        {
        	SDL_Color f;
        	SDL_Color b;
        };
        // Glyph
        struct glyph
        {
        	color col;
			char chr;
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
        int elapsed_frames = 0; // Frames since game was started
        int frame_step_precision = 10000000; // Precision of sleep time to ensure the game stays in sync
        char def_char = ' '; // Default character to clean the graphics arrays
        color def_color = {{255,255,255}, {0,0,0}}; // Default color to clean the color arrays
        int def_col = 0; // Default collision value to clean the collision arrays with
        int font_w; // Width of the font
        int font_disp_x = 0;
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
        int scr_w; // W of screen
        int scr_h; // H of screen
        int SDL_window_props = SDL_WINDOW_FULLSCREEN; //0;
        std::string font_path;
        SDL_Window* window = NULL; // Pointer to the SDL window object
        SDL_Renderer* renderer = NULL; // Pointer to the SDL renderer object
        TTF_TextEngine* engine = NULL; // Point to the SDL_ttf text engine (only used if glyph_precaching is off)
        TTF_Font* sans = NULL; // SDL_ttf font to use
        TTF_Text **draw_chars = NULL; // List of character objects that are drawn in a grid (only used if glyph_precaching is off)
        std::unordered_map<std::string, SDL_Texture*> character_textures = {}; // Texture cache (only used if glyph_precaching is on)
        std::unordered_map<std::string, int> character_widths = {}; // Texture width cache (only used if glyph_precaching is on)
        std::vector<glyph> glyph_buffer = {}; // Glyphs to be loaded (only used if glyph_precaching is on)
        // Glyph pre-caching changes how the drawing pipeline works.
        // If this setting is off, the engine will render text on the
        // fly using the TTF text engine. However this does struggle at higher
        // quantities of characters on screen.
        // Glyph pre-caching allows you to tell the game engine ahead of time what
        // character textures you intend to use ahead of time and then writes the
        // resulting textures to a map. This method of drawing is better at drawing
        // a lot of characters at once, but may slow down with a larger variety of
        // character textures. Additionally, initially loading characters into the buffer
        // is process intensive. As such, it is best to only modify the glyph buffer
        // at the start of the game or during loading screens or room transitions.
        bool glyph_prechaching;

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
        GravityEngine_Core(const char* gt, const char* gi, const char* gv, int cw, int ch, int fw, int fh, int f, bool q, int sw, int sh, std::string fp)
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

            // Set glyph_prechaching or not
            glyph_prechaching = q;

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
            for (int i = 0; i < canvas_w * canvas_h; i++)
            {
                buf_char_screen[i] = ' ';
                buf_col_screen[i] = {{255,255,255},{0,0,0}};
            }
            // Set the desired frame length to 1 second divided be the desired frame rate
            frame_length = 1000000000 / f;
        }

        // Gravity Engine Constructor
        // int cw : window width
        // int ch : window height
        // int f : frame rate cap in frames per second
        GravityEngine_Core(const char* gt, const char* gi, const char* gv, int cw, int ch, int f, bool q, int w, int h, std::string fp) : GravityEngine_Core(gt, gi, gv, cw, ch, -1, -1, f, q, w, h, fp) {}

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
            if (SDL_Init(SDL_INIT_VIDEO ) == false)
            {
                std::cout << SDL_GetError() << std::endl;
                std::system("pause");
            }
            // Create the SDL window
            SDL_CreateWindowAndRenderer(game_title, canvas_w * font_w, canvas_h * font_h, SDL_window_props, &window, &renderer);
            engine = TTF_CreateRendererTextEngine(renderer);

            // Init SDL ttf
            TTF_Init();

            // Create font
            const char * fp = font_path.c_str();
            sans = TTF_OpenFont(fp, font_h);

            // Create texts
            if (!glyph_prechaching)
            {
                draw_chars = new TTF_Text*[canvas_w * canvas_h];
                for (int i = 0; i < canvas_w * canvas_h; i++)
                	draw_chars[i] = TTF_CreateText(engine, sans, "0", 0u);
            }
            else
            {
				// Cache all of the glyph textures
				CacheGlyphTextures();
            }

            // Call init custom user code
            if (init_game != nullptr)
                init_game();

            // Call game loop
            GameLoop(pre_loop_code, post_loop_code);

            // TTF Quit
            if (!glyph_prechaching)
            {
				for (int i = 0; i < canvas_w * canvas_h; i++)
					TTF_DestroyText(draw_chars[i]);
				delete[] draw_chars;
            }
            else
            {
            	for (auto t : character_textures)
            	{
            		SDL_DestroyTexture(t.second);
            	}
            }
			TTF_DestroyRendererTextEngine(engine);
            TTF_Quit();

            // Kill SDL
            SDL_Quit();

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
            // Create font
            const char * fp = fpth.c_str();
            sans = TTF_OpenFont(fp, font_h);
            // Re-render glyphs
            if (glyph_prechaching)
            	CacheGlyphTextures();
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

        // Get Key State
        // TODO: Implement SDL input
        bool GetKey(int key_code)
        {
            return false; // (GetKeyState(key_code) & 0x8000);
        }

        // Add a glyph to the glyph buffer to be loaded into the texture cache
        void AddGlyph(glyph g, bool apply_immediatly = false)
		{
        	glyph_buffer.push_back(g);
        	if (apply_immediatly) CacheGlyph(g.chr, g.col);
		}

        // Add a glyph to the glyph buffer to be loaded into the texture cache
        void SetAllGlyphs(std::vector<glyph> g, bool apply_immediatly = false)
		{
        	glyph_buffer.clear();
        	for (auto gly : g) { glyph_buffer.push_back(gly); }
        	if (apply_immediatly) CacheGlyphTextures();
		}

        // Store all glyphs to be used in the future
        void CacheGlyphTextures()
        {
        	// Delete all existing character textures
        	for (auto t : character_textures)
        	{
        		SDL_DestroyTexture(t.second);
        	}
        	// Clear the text caches
        	character_textures.clear();
        	character_widths.clear();
        	// Render and cache characters present in the glyph buffer
        	for (auto g : glyph_buffer)
            	CacheGlyph(g.chr, g.col);
        }

        // Add the object to the entity list
        void AddObject(GravityEngine_Object* object)
        {
        	entity_list.push_back(object);
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

        // Render the texture for the given character with the given color into texture cache
        void CacheGlyph(char i, color c)
        {
			std::string str_key = (char)i
								  + std::to_string(c.f.r)
								  + std::to_string(c.f.g)
								  + std::to_string(c.f.b)
								  + std::to_string(c.b.r)
								  + std::to_string(c.b.g)
								  + std::to_string(c.b.b);

			// Create texture
			SDL_Surface* surf_message = TTF_RenderGlyph_Blended(sans, (char)i,
					{c.f.r,c.f.g,c.f.b});
			// Create color background
			SDL_Surface* surf_color = SDL_CreateSurface(font_w, font_h, surf_message->format);
			SDL_ClearSurface(surf_color, ((double)c.b.r)/255.0, ((double)c.b.g)/255.0, ((double)c.b.b)/255.0, 255);

			// Merge
			SDL_BlitSurface(surf_message, NULL, surf_color, NULL);

			character_textures[str_key] = SDL_CreateTextureFromSurface(renderer, surf_color);
			character_widths[str_key] = surf_color->w;

			SDL_DestroySurface(surf_message);
			SDL_DestroySurface(surf_color);
        }

        // Draw screen buffer to the SDL window
        void DrawScreenText()
        {
        	// Init the iterator for the character buffer
        	int buf_index = 0;
        	// Init the x and y coords for the output
        	int x = 0;
        	int y = 0;
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
            	if (!glyph_prechaching)
            	{
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
					// Draw glyph
					SDL_FRect rect;
					rect.x = x*font_disp_x;
					rect.y = y*font_h;
					rect.w = font_w;
					rect.h = font_h;
					SDL_SetRenderDrawColor(renderer,
							buf_col_screen[buf_index].b.r,
							buf_col_screen[buf_index].b.g,
							buf_col_screen[buf_index].b.b,
							255);
				    SDL_RenderFillRect(renderer, &rect);
					// Render the text
					TTF_DrawRendererText(draw_chars[buf_index], x*font_w, y*font_h);
            	}
            	else
            	{
					// Create key string
            		/*
            		std::ostringstream str_key;
            		str_key << buf_char_screen[buf_index]
						    << (int)(buf_col_screen[buf_index].f.r)
						    << (int)(buf_col_screen[buf_index].f.g)
						    << (int)(buf_col_screen[buf_index].f.b)
						    << (int)(buf_col_screen[buf_index].b.r)
						    << (int)(buf_col_screen[buf_index].b.g)
						    << (int)(buf_col_screen[buf_index].b.b);
					*/
            		// A255255255255255255
            		std::string str_key;
            		str_key += buf_char_screen[buf_index];
					str_key += std::to_string((int)(buf_col_screen[buf_index].f.r));
					str_key += std::to_string((int)(buf_col_screen[buf_index].f.g));
					str_key += std::to_string((int)(buf_col_screen[buf_index].f.b));
					str_key += std::to_string((int)(buf_col_screen[buf_index].b.r));
					str_key += std::to_string((int)(buf_col_screen[buf_index].b.g));
					str_key += std::to_string((int)(buf_col_screen[buf_index].b.b));

					// Draw glyph
					SDL_FRect message_rect;
					message_rect.x = x*font_disp_x;
					message_rect.y = y*font_h;
					message_rect.w = character_widths[str_key];
					message_rect.h = font_h;
					SDL_RenderTexture(renderer, (SDL_Texture*)character_textures[str_key], NULL, &message_rect);
            	}

                // Inc vars
            	x++;
            	buf_index++;
            }
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
            SDL_RenderClear(renderer);

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

            // Draw to the window
            SDL_RenderPresent(renderer);
            SDL_Delay(0);

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
                DrawTextString(0,0,debug,"Delta Time: " + std::to_string(DeltaTime()) + " Elapsed Seconds: " + std::to_string(seconds),{{255,255,255},{0,0,0}});
                DrawTextString(0,1,debug,"Frame Time: " + std::to_string(frame_time) + " FPS: " + std::to_string(*frames_per_second),{{255,255,255},{0,0,0}});
                if (glyph_prechaching)
                	DrawTextString(0,2,debug,"Cached Textures: " + std::to_string(character_widths.size()),{{255,255,255},{0,0,0}});
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
