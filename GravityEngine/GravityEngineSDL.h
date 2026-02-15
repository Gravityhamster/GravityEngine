#include "GravitySynthSDL.h"
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <SDL3_image/SDL_image.h>
#include <iostream>
#include <fstream>
#include <chrono>
#include <math.h>
#include <future>
#include <thread>
#include <vector>
#include <string>
#include <unordered_map>
#include <random>


// Color struct (foreground and background)
// SDL_Color f : Letter color
// SDL_Color b : Background color
struct color
{
    SDL_Color f;
    SDL_Color b;
};

// Enum to define the type of sound currently playing on the channel
enum ChannelType
{
    file,
    synth
};

// Template for game objects
class GravityEngine_Object
{
public:
    GravityEngine_Object() {}; // Constructor
    virtual ~GravityEngine_Object() {}; // Destructor
    virtual void begin_step() {}; // Code to run at the start of the frame
    virtual void step() {}; // Code to run during the frame
    virtual void end_step() {}; // Code to run at the end of the frame
};

// Core engine class
class GravityEngine_Core
{
    // Gravity Engine public types
public:
    // Enum to define which pixel-based graphical layer to work in
    enum sprite_layer
    {
        ui, foreground, background, entity, debug
    };
    // Enum to define which collision layer to work in
    enum col_layer
    {
        stat, dyn
    };

    // Gravity Engine private types
private:

    // Gravity Engine private classes
private:

    // Gravity Engine sound class
    class GravityEngine_Sound
    {
    private:
        // Copilot help on this one
        // Convert the audio in the audio buffer to a different audio spec
        // Uint8* audio_buf : The buffer for the audio data
        // Uint32 audio_len : The length of the audio in the audio buffer
        // SDL_AudioSpec wav_audio_spec : Audio specifications of the audio to be converted
        // SDL_AudioSpec audio_spec : Audio specifications to convert the audio to
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

    public:
        // -= Attributes =-
        std::vector<Uint8> converted_audio; // Buffer for the final converted audio data

        // -= Methods =-

        // Construct audio
        // const char* path : File path of the audio
        // SDL_AudioSpec audio_spec : Audio specification to convert the audio to (this should be the global audio spec in the engine)
        GravityEngine_Sound(const char* path, SDL_AudioSpec audio_spec)
        {
            Uint8* audio_buf;
            Uint32 audio_len;
            SDL_AudioSpec wav_audio_spec;
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
        SDL_AudioStream* sdl_audio_stream = nullptr; // SDL audio streaming object
        SDL_AudioDeviceID audio_device_id; // SDL audio playback device object
        ChannelStates state = uninit; // Playback state of this audio channel
        ChannelType type = file; // Sound type currently playing
        std::vector<Uint8>* currently_playing_audio = nullptr; // Saved audio for feeding loop
        bool looping = false; // Loop audio
        std::thread* synth_thread;
        bool synth_playing = false;

    public:

        // -= Methods =-

        // Construct audio
        GravityEngine_AudioChannel(SDL_AudioSpec audio_spec)
        {
            // Open the audio device for playback
            audio_device_id = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
            // Create the audio stream
            sdl_audio_stream = SDL_CreateAudioStream(&audio_spec, &audio_spec);
            // Flag the the audio channel is ready for playback
            state = init;
        };

        // Play a sound on this channel
        // SDL_AudioSpec audio_spec : Audio specification to play the audio at (this should probably be the global audio spec in the engine)
        // GravityEngine_Sound* gravity_engine_sound_ref : Audio sound file data container
        // bool loop : Whether or not audio should loop indefinitely
        void PlaySound(SDL_AudioSpec audio_spec, GravityEngine_Sound* gravity_engine_sound_ref, bool loop = false)
        {
            // If any audio is currently playing, stop it
            StopPlayback();
            // Place the audio into the audio stream to be played on the channel
            SDL_PutAudioStreamData(sdl_audio_stream, gravity_engine_sound_ref->converted_audio.data(), gravity_engine_sound_ref->converted_audio.size());
            // Attach the audio stream to the channel's audio device
            SDL_BindAudioStream(audio_device_id, sdl_audio_stream);
            // Start playback
            SDL_ResumeAudioDevice(audio_device_id);
            // Flag that this sound channel is busy playing
            state = playing;
            // Set whether this channel should loop or not
            looping = loop;
            // Save the audio bound to this channel so we can feed the loop later
            if (loop) currently_playing_audio = &gravity_engine_sound_ref->converted_audio;
            // Change the provider type
            type = file;
        }

        // Play a synth on this channel
        // SDL_AudioSpec audio_spec : Audio specification to play the audio at (this should probably be the global audio spec in the engine)
        // GravityEngine_Synth* gravity_engine_synth_ref : Audio sound file data container
        void PlaySynth(SDL_AudioSpec* audio_spec, GravityEngine_Synth* gravity_engine_synth_ref)
        {
            // If any audio is currently playing, stop it
            StopPlayback();
            // Flag that this sound channel is busy playing
            state = playing;
            // Start synth thread
            synth_playing = true;
            std::thread st(GravityEngine_Synth::GenerateAudio, gravity_engine_synth_ref, sdl_audio_stream, audio_spec, audio_device_id, &state, &synth_playing);
            st.detach();
            synth_thread = &st;
            // Attach the audio stream to the channel's audio device
            SDL_BindAudioStream(audio_device_id, sdl_audio_stream);
            // Start playback
            SDL_ResumeAudioDevice(audio_device_id);
            // Set whether this channel should loop or not
            looping = true;
            // Change the provider type
            type = synth;
        }

        // Stop audio
        void StopPlayback()
        {
            // Flag that this sound channel has been stopped and cleared
            state = stopped;
            // Wait for the synth thread if this is a synth
            if (type == synth)
            {
                // Wait for the thread to quit
                while (synth_playing) {}
            }
            // Stop the playback
            SDL_PauseAudioDevice(audio_device_id);
            // Empty the audio stream data
            SDL_ClearAudioStream(sdl_audio_stream);
            // If this channel was set to loop, set it to stop looping
            looping = false;
        }

        // Pause audio
        void PausePlayback()
        {
            // Pause the playback, but do not forget the position
            SDL_PauseAudioDevice(audio_device_id);
            // Flag that this sound channel has been paused
            state = paused;
        }

        // Continue audio
        void ResumePlayback()
        {
            // Continue audio playback
            SDL_ResumeAudioDevice(audio_device_id);
            // Flag that this sound channel is busy playing
            state = playing;
        }

        // Cancel loop
        void CancelLoop()
        {
            // Stop looping without stopping the sound altogether
            looping = false;
        }

        // Feed the channel with loop audio
        void FeedLoop()
        {
            // Is this channel truly supposed to loop?
            if (looping == true && state == playing)
            {
                auto size_current = SDL_GetAudioStreamQueued(sdl_audio_stream);
                auto size_of_sample = currently_playing_audio->size() * sizeof(Uint8);
                // Do not feed if the stream has plenty enough data. 
                // Only read if the data in the stream is less than the size of the sample.
                if (size_current < size_of_sample)
                    // Feed the sdl_audio_stream
                    SDL_PutAudioStreamData(sdl_audio_stream, currently_playing_audio->data(), currently_playing_audio->size());
            }
        }

        // Get state of channel
        ChannelStates GetState()
        {
            return state;
        }

        // Get type of channel
        ChannelType GetType()
        {
            return type;
        }

        // Destruct audio
        ~GravityEngine_AudioChannel()
        {
            // Stop the playback
            SDL_PauseAudioDevice(audio_device_id);
            // Flag that this sound channel has been stopped and cleared
            state = stopped;
            // Wait for the synth thread if this is a synth
            if (type == synth)
            {
                // Wait for the thread to quit
                while (synth_playing) {}
            }
            SDL_Delay(5);
            currently_playing_audio = nullptr;
            SDL_CloseAudioDevice(audio_device_id);
        };
    };

    // Gravity Engine Private Attributes
private:
    struct SDL_AudioSpec global_audio_spec; // = { SDL_AUDIO_S32LE,2,48000 }; // Set the format that all audio should be converted to
    std::vector<GravityEngine_Object*> entity_list; // This is the list of GravityEngine objects that the engine will track and execute
    bool game_running = false; // Is the game running or no?
    int canvas_w; // Game canvas width
    int canvas_h; // Game canvas height
    char** collision_static; // Game static collision layer
    char** collision_dynamic; // Game dynamic collision layer
    int elapsed_frames = 0; // Frames since game was started
    color def_color = { {255,255,255}, {0,0,0} }; // Default color to clean the color arrays
    int def_col = 0; // Default collision value to clean the collision arrays with
    int font_w; // Width of the font
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
    SDL_Texture* render_texture_ui = NULL; // The texture for the entire screen
    TTF_TextEngine* engine = NULL; // Point to the SDL_ttf text engine
    TTF_Font* sans = NULL; // SDL_ttf font to use
    SDL_Texture* p_background_texture = NULL; // Background sprite layer
    SDL_Texture* p_entity_texture = NULL; // Enttiy sprite layer
    SDL_Texture* p_foreground_texture = NULL; // Foreground sprite layer
    SDL_Texture* p_ui_texture = NULL; // Ui sprite layer
    SDL_Texture* p_debug_texture = NULL; // Debug sprite layer
    const bool* keyboard_keys = SDL_GetKeyboardState(NULL); // Initialize keystate list
    std::vector<GravityEngine_AudioChannel*> audio_channels; // List of all audio channels
    std::vector<GravityEngine_Sound*> sounds; // List of all saved sounds
    int channels; // Channel count
    int mouse_wheel_state; // Store the current 
    std::ofstream file_out = std::ofstream("output.txt");
    std::vector<SDL_Texture*> sprite_list; // List of sprite resources loaded into the game

    // Gravity Engine Public Attributes
public:
    bool debug_mode = false; // Show debug overlay
    bool debug_complex = false; // Show complex debug overlay
    int cam_offset_x = 0;
    int cam_offset_y = 0;

    // Gravity Engine Public Methods
public:

    // Gravity Engine Constructor
    // const char* gt : Game title
    // const char* gi : Game id
    // const char* gv : Game version
    // int cw : Canvas width (chars wide)
    // int ch : Canvas height (chars tall)
    // int fw : Font width (-1 for auto-detect)
    // int fh : Font height (-1 for auto-detect)
    // int f : Frame rate cap in frames per second
    // int sw : Screen width
    // int sh : Screen height
    // string fp : Path to the display font
    // int c : Number of audio channels
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

        // Instantiate collision static
        collision_static = new char* [canvas_h * 2];
        for (int i = 0; i < canvas_h * 2; i++)
            collision_static[i] = new char[canvas_w * 2];
        for (int i = 0; i < canvas_h * 2; i++)
            for (int q = 0; q < canvas_w * 2; q++)
                collision_static[i][q] = def_col;

        // Instantiate collision dynamic
        collision_dynamic = new char* [canvas_h * 2];
        for (int i = 0; i < canvas_h * 2; i++)
            collision_dynamic[i] = new char[canvas_w * 2];
        for (int i = 0; i < canvas_h * 2; i++)
            for (int q = 0; q < canvas_w * 2; q++)
                collision_dynamic[i][q] = def_col;

        // Set the desired frame length to 1 second divided be the desired frame rate
        frame_length = 1000000000 / f;
        // Set channel count
        channels = c;
    }

    // Gravity Engine Constructor
    // const char* gt : Game title
    // const char* gi : Game id
    // const char* gv : Game version
    // int f : Frame rate cap in frames per second
    // int w : Screen width
    // int h : Screen height
    // string fp : Path to the display font
    // int c : Number of audio channels
    GravityEngine_Core(const char* gt, const char* gi, const char* gv, int cw, int ch, int f, int w, int h, std::string fp, int c) : GravityEngine_Core(gt, gi, gv, cw, ch, -1, -1, f, w, h, fp, c) {}

    // Get the canvas width
    int GetCanvasW()
    {
        return canvas_w;
    }

    // Get the canvas height
    int GetCanvasH()
    {
        return canvas_h;
    }

    // Get the collision type at the given location
    // int x : Horizontal coordinate
    // int y : Vertical coordinate
    // col_layer cl : Layer to get collision from
    int GetCollisionValue(int x, int y, col_layer cl)
    {
        if (x >= 0 && x < canvas_w * 2 && y >= 0 && y < canvas_h * 2)
        {
            if (cl == stat)
                return collision_static[y][x];
            else
                return collision_dynamic[y][x];
        }
        else
        {
            return NULL;
        }
    }

    // Set the collision type at the given location
    // int x : Horizontal coordinate
    // int y : Vertical coordinate
    // col_layer cl : Layer to set collision on
    // int v : Collision type value
    void SetCollisionValue(int x, int y, col_layer cl, int v)
    {
        if (x >= 0 && x < canvas_w * 2 && y >= 0 && y < canvas_h * 2)
        {
            if (cl == stat)
                collision_static[y][x] = v;
            else if (cl == dyn)
                collision_dynamic[y][x] = v;
        }
    }

    // Gravity Engine Destructor
    ~GravityEngine_Core()
    {
        // Delete all of the layers
        delete[] collision_static;
        delete[] collision_dynamic;
    }

    // Start the video game
    // void (*init_game)() : Custom game initialization function
    // void (*pre_loop_code)() : Custom global begin-step function
    // void (*post_loop_code)() : Custom global end-step function
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

        // Load the audio spec
        auto dev = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
        SDL_GetAudioDeviceFormat(dev, &global_audio_spec, nullptr);
        SDL_CloseAudioDevice(dev);

        // Initialize all audio channels
        for (int i = 0; i < channels; i++)
            audio_channels.insert(audio_channels.end(), new GravityEngine_AudioChannel(global_audio_spec));

        // Create the render texture
        render_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, scr_w * 4, scr_h * 4);
        render_texture_ui = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, scr_w, scr_h);
        p_background_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, scr_w * 2, scr_h * 2);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_SetRenderTarget(renderer, p_background_texture);
        SDL_RenderClear(renderer);
        p_foreground_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, scr_w * 2, scr_h * 2);
        p_entity_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, scr_w * 2, scr_h * 2);
        p_ui_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, scr_w, scr_h);
        p_debug_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, scr_w, scr_h);

        // Create the engine used to write text
        engine = TTF_CreateRendererTextEngine(renderer);

        // Init SDL ttf
        TTF_Init();

        // Create font
        const char* fp = font_path.c_str();
        sans = TTF_OpenFont(fp, font_h);

        // Call init custom user code
        if (init_game != nullptr)
            init_game();

        // Call game loop
        GameLoop(pre_loop_code, post_loop_code);

        // -= GAME END =-
        // Clenup goes here

        // TTF Quit
        TTF_DestroyRendererTextEngine(engine);
        TTF_Quit();

        // Free audio channels
        for (auto ac : audio_channels)
            delete ac;
        for (auto s : sounds)
            delete s;

        // Kill SDL
        SDL_Quit();

        // Free all objects
        for (auto o : entity_list)
            delete o;
        for (auto s : sprite_list)
            SDL_DestroyTexture(s);

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
        double t = ((frame_time * 1.0) / (frame_length * 1.0));
        return t;
    }

    // Get current fps
    int fps_now()
    {
        return current_fps;
    }

    // Change Font
    // string fpth : Path to the font file
    void ChangeFont(std::string fpth)
    {
        // Create font
        const char* fp = fpth.c_str();
        sans = TTF_OpenFont(fp, font_h);
    }

    // Get the width of the font grid
    int GetFontW()
    {
        return font_w;
    }

    // Get the height of the font grid
    int GetFontH()
    {
        return font_h;
    }

    // Get the width of the font grid
    int GetScreenW()
    {
        return scr_w;
    }

    // Get the height of the font grid
    int GetScreenH()
    {
        return scr_h;
    }

    // Get elapsed_frames
    long GetElapsedFrames()
    {
        return elapsed_frames;
    }

    // Handle input
    // SDL_Scancode sdlKey : Keycode to check state
    bool GetKeyState(SDL_Scancode sdlKey)
    {
        SDL_PumpEvents();
        if (keyboard_keys[sdlKey])
            return true;
        else
            return false;
    }

    // Handle mouse input
    // SDL_MouseButtonFlags sdlButton : Button to check state
    bool GetMouseButtonState(SDL_MouseButtonFlags sdlButton)
    {
        SDL_MouseButtonFlags Buttons{ SDL_GetMouseState(NULL, NULL) };
        if (Buttons & SDL_BUTTON_MASK(sdlButton))
            return true;
        else
            return false;
    }

    // Handle mouse wheel
    int GetMouseWheelState()
    {
        return mouse_wheel_state;
    }

    // Handle mouse location
    // float* ret_x : Pointer to store the horizontal position
    // float* ret_y : Pointer to store the vertical position
    void GetMousePosition(float* ret_x, float* ret_y)
    {
        float x, y;
        SDL_GetMouseState(&x, &y);

        *ret_x = (x / scr_w) * canvas_w;
        *ret_y = (y / scr_h) * canvas_h;
    }

    // Get a random number - https://www.geeksforgeeks.org/cpp/how-to-generate-random-number-in-range-in-cpp/
    // int min : Minimum number to get random value in 
    // int max : Maximum number to get random value in 
    int RandRange(int min, int max)
    {
        // Initialize a random number generator
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> distrib(min, max);
        // Return value
        return distrib(gen);
    }

    // Add the object to the entity list
    // GravityEngine_Object* object : Gravity engine managed object reference
    int AddObject(GravityEngine_Object* object)
    {
        entity_list.insert(entity_list.end(), object);
        return entity_list.size() - 1;
    }

    // Remove the object from the entity list
    // GravityEngine_Object* object : Gravity engine managed object reference
    void RemoveObject(GravityEngine_Object* object)
    {
        std::erase(entity_list, object);
    }

    // Add the sprite to the sprite list
    // const char* sprite_path : File path to the sprite to be loaded
    // SDL_ScaleMode scale_mode : Antialiasing type
    int AddSprite(const char* sprite_path, SDL_ScaleMode scale_mode = SDL_SCALEMODE_NEAREST)
    {
        auto sprite = IMG_Load(sprite_path);
        auto texture = SDL_CreateTextureFromSurface(renderer, sprite);
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
        SDL_DestroySurface(sprite);
        sprite_list.insert(sprite_list.end(), texture);
        return sprite_list.size() - 1;
    }

    // Delete sprite from the sprite list
    // int index : Integer index to where the sprite is stored
    void DeleteSprite(int index)
    {
        // Delete the texture
        SDL_DestroyTexture(sprite_list[index]);
        // Set this index to a nullptr
        sprite_list[index] = nullptr;
    }

    // Draw a sprite at a location
    // int index : Integer index to where the sprite is stored
    // double x : Horizontal position of sprite
    // double y : Vertical position of sprite
    // double w_scale : Horizontal scaling of sprite
    // double H_scale : Vertical scaling of sprite
    // sprite_layer l : Layer to draw the sprite on
    void DrawSprite(int index, double x, double y, double w_scale, double h_scale, sprite_layer l)
    {
        SDL_Texture* rt = p_entity_texture;
        // Set the target layer
        switch (l)
        {
        case background:
            rt = p_background_texture;
            break;
        case entity:
            rt = p_entity_texture;
            break;
        case foreground:
            rt = p_foreground_texture;
            break;
        case ui:
            rt = p_ui_texture;
            break;
        case debug:
            rt = p_debug_texture;
            break;
        }
        SDL_SetRenderTarget(renderer, rt);
        // Get the dimensions of the character
        float w, h;
        SDL_GetTextureSize(sprite_list[index], &w, &h);
        // Create an FRect to draw to
        SDL_FRect dst = { x, y, w * w_scale, h * w_scale };
        // Render the sprite to the graphical layer
        SDL_RenderTexture(renderer, sprite_list[index], NULL, &dst);
        // Re-render if wrap
        if (x + w * w_scale > scr_w * 2)
        {
            // Create an FRect to draw to
            SDL_FRect dst_w = { x - scr_w * 2, y, w * w_scale, h * w_scale };
            // Render the sprite to the graphical layer
            SDL_RenderTexture(renderer, sprite_list[index], NULL, &dst_w);
        }
        if (y + h * h_scale > scr_h * 2)
        {
            // Create an FRect to draw to
            SDL_FRect dst_w = { x, y - scr_h * 2, w * w_scale, h * w_scale };
            // Render the sprite to the graphical layer
            SDL_RenderTexture(renderer, sprite_list[index], NULL, &dst_w);
        }
        if (y + h * h_scale > scr_h * 2 && x + w * w_scale > scr_w * 2)
        {
            // Create an FRect to draw to
            SDL_FRect dst_w = { x - scr_w * 2, y - scr_h * 2, w * w_scale, h * w_scale };
            // Render the sprite to the graphical layer
            SDL_RenderTexture(renderer, sprite_list[index], NULL, &dst_w);
        }
        if (x < 0 && y < 0)
        {
            // Create an FRect to draw to
            SDL_FRect dst_w = { x + scr_w * 2, y + scr_h * 2, w * w_scale, h * w_scale };
            // Render the sprite to the graphical layer
            SDL_RenderTexture(renderer, sprite_list[index], NULL, &dst_w);
        }
        if (x < 0)
        {
            // Create an FRect to draw to
            SDL_FRect dst_w = { x + scr_w * 2, y, w * w_scale, h * w_scale };
            // Render the sprite to the graphical layer
            SDL_RenderTexture(renderer, sprite_list[index], NULL, &dst_w);
        }
        if (y < 0)
        {
            // Create an FRect to draw to
            SDL_FRect dst_w = { x, y + scr_h * 2, w * w_scale, h * w_scale };
            // Render the sprite to the graphical layer
            SDL_RenderTexture(renderer, sprite_list[index], NULL, &dst_w);
        }
        // Notofy the drawing pipeline that a change has been made
        screen_updated = true;
    }

    // Draw a rectangle
    void DrawRect(double x, double y, double w, double h, SDL_Color c, sprite_layer l)
    {
        SDL_Texture* rt = p_entity_texture;
        // Set the target layer
        switch (l)
        {
        case background:
            rt = p_background_texture;
            break;
        case entity:
            rt = p_entity_texture;
            break;
        case foreground:
            rt = p_foreground_texture;
            break;
        case ui:
            rt = p_ui_texture;
            break;
        case debug:
            rt = p_debug_texture;
            break;
        }
        SDL_SetRenderTarget(renderer, rt);
        // Create an FRect to draw to
        SDL_FRect fr = { x, y, w, h };
        // Fill the rectangle with color
        SDL_SetRenderDrawColor(renderer,
            c.r,
            c.g,
            c.b,
            c.a);
        SDL_RenderFillRect(renderer, &fr);
        // Render the rectangle
        SDL_RenderRect(renderer, &fr);
        // Notify the drawing pipeline that a change has been made
        screen_updated = true;
    }

    // Add sounds to the sound list
    // const char* path : Path to sound file
    int AddSound(const char* path)
    {
        // Initialize all audio channels
        sounds.insert(sounds.end(), new GravityEngine_Sound(path, global_audio_spec));
        return sounds.size() - 1;
    }

    // Delete sound from the sound list
    // int index : Integer index to where the sound is stored
    void DeleteSound(int index)
    {
        // Delete the sound objects
        delete[] sounds[index];
        // Set this index to a nullptr
        sounds[index] = nullptr;
    }

    // Play a sound on a channel
    // int audio_index : Integer index to where the sound is stored
    // int channel : Integer channel index to play the sound at the index on
    // bool loop : Whether the sound should loop or not
    void PlaySoundOnChannel(int audio_index, int channel, bool loop = false)
    {
        channel = channel % audio_channels.size();
        audio_channels[channel]->PlaySound(global_audio_spec, sounds[audio_index], loop);
    }

    // Bind a synth to a channel
    // int audio_index : Integer index to where the sound is stored
    // int channel : Integer channel index to play the sound at the index on
    // bool loop : Whether the sound should loop or not
    void BindSynthToChannel(GravityEngine_Synth* s, int channel)
    {
        channel = channel % audio_channels.size();
        audio_channels[channel]->PlaySynth(&global_audio_spec, s);
    }

    // Pause channel
    // int channel : Integer channel index
    void PauseChannel(int channel)
    {
        audio_channels[channel]->PausePlayback();
    }

    // Resume channel
    // int channel : Integer channel index
    void ResumeChannel(int channel)
    {
        audio_channels[channel]->ResumePlayback();
    }

    // Stop channel
    // int channel : Integer channel index
    void StopChannel(int channel)
    {
        audio_channels[channel]->StopPlayback();
    }

    // Cancel channel's looping status
    // int channel : Integer channel index
    void CancelChannelLoop(int channel)
    {
        audio_channels[channel]->CancelLoop();
    }

private:

    // Draw screen buffer to the SDL window
    void DrawScreen()
    {
        // Render all layers to the render_texture
        if (screen_updated)
        {
            // Define where the layer will go in a rect
            for (int q = 0; q <= 1; q++)
            {
                for (int i = 0; i <= 1; i++)
                {
                    SDL_FRect c_rect;
                    c_rect.x = scr_w * i * 2;
                    c_rect.y = scr_h * q * 2;
                    c_rect.w = scr_w * 2;
                    c_rect.h = scr_h * 2;
                    SDL_FRect s_rect;
                    s_rect.x = 0;
                    s_rect.y = 0;
                    s_rect.w = scr_w * 2;
                    s_rect.h = scr_h * 2;
                    // Render to texture instead of directly to the screen
                    SDL_SetRenderTarget(renderer, render_texture);
                    // Draw the background text texture to the renderer
                    SDL_RenderTexture(renderer, p_background_texture, new SDL_FRect(s_rect), new SDL_FRect(c_rect));
                    // Draw the background text texture to the renderer
                    SDL_RenderTexture(renderer, p_entity_texture, new SDL_FRect(s_rect), new SDL_FRect(c_rect));
                    // Draw the foreground text texture to the renderer
                    SDL_RenderTexture(renderer, p_foreground_texture, new SDL_FRect(s_rect), new SDL_FRect(c_rect));
                }
            }

            SDL_FRect d_rect;
            d_rect.x = 0;
            d_rect.y = 0;
            d_rect.w = scr_w;
            d_rect.h = scr_h;
            SDL_FRect t_rect;
            t_rect.x = 0;
            t_rect.y = 0;
            t_rect.w = scr_w;
            t_rect.h = scr_h;
            // Render to texture instead of directly to the screen
            SDL_SetRenderTarget(renderer, render_texture_ui);
            // Draw the ui text texture to the renderer
            SDL_RenderTexture(renderer, p_ui_texture, new SDL_FRect(t_rect), new SDL_FRect(d_rect));
            // Draw the debug text texture to the renderer
            SDL_RenderTexture(renderer, p_debug_texture, new SDL_FRect(t_rect), new SDL_FRect(d_rect));
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

        // Handle looping audio channels
        for (auto ac : audio_channels)
            if (ac->GetType() == file)
                ac->FeedLoop();

        // Poll SDL
        SDL_Event event;
        mouse_wheel_state = 0;
        while (SDL_PollEvent(&event)) {
            // Get close event
            if (event.type == SDL_EVENT_QUIT)
                game_running = false;
            if (event.type == SDL_EVENT_MOUSE_WHEEL)
            {
                mouse_wheel_state = event.wheel.y;
            }
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

        // Draw visuals
        DrawScreen();
    }

    // Post-game code
    void SystemPostGameLoop()
    {
        // Frame count
        elapsed_frames++;

        // Mod
        if (cam_offset_x <= 0)
            cam_offset_x += scr_w * 2;
        if (cam_offset_x >= scr_w * 2)
            cam_offset_x -= scr_w * 2;
        if (cam_offset_y <= 0)
            cam_offset_y += scr_h * 2;
        if (cam_offset_y >= scr_h * 2)
            cam_offset_y -= scr_h * 2;

        // Draw to the window - Do not draw if the draw flag is off
        if (screen_updated)
        {
            SDL_FRect d_rect;
            d_rect.x = 0;
            d_rect.y = 0;
            d_rect.w = scr_w;
            d_rect.h = scr_h;
            SDL_FRect s_rect;
            s_rect.x = cam_offset_x;
            s_rect.y = cam_offset_y;
            s_rect.w = scr_w;
            s_rect.h = scr_h;
            // Draw the screen texture to the renderer
            SDL_RenderTexture(renderer, render_texture, &s_rect, NULL);
            SDL_RenderTexture(renderer, render_texture_ui, &d_rect, NULL);
            SDL_RenderPresent(renderer);
            // I dunno why I have this delay here
            SDL_Delay(0);
            // Reset the draw flag
            screen_updated = false;
            // Clear output
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
            SDL_RenderClear(renderer);
        }

        // Clear the Dynamic Collision values
        for (int i = 0; i < canvas_h * 2; i++)
        {
            for (int q = 0; q < canvas_w * 2; q++)
            {
                SetCollisionValue(q, i, dyn, 0);
            }
        }

        // Clear the Entity and Debug pixel layers
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
        SDL_SetRenderTarget(renderer, p_debug_texture);
        SDL_RenderClear(renderer);
        SDL_SetRenderTarget(renderer, p_entity_texture);
        SDL_RenderClear(renderer);
        SDL_SetRenderTarget(renderer, render_texture_ui);
        SDL_RenderClear(renderer);

        // Call all step functions
        for (auto o : entity_list)
            (*o).end_step();
    }

    // Log timing
    // long* frame_check : Elapsed frames since the last second 
    //                     (Basically, this counts up, then when a second passes, 
    //                     it sets this to zero and counts up again, 
    //                     thus giving you the frames per second 
    //                     [frames drawn per second])
    // long* second_check : Tracker variable for determining if we should update the FPS number or not
    // long* frames_per_second : FPS tracker variable
    void LogFrameTimingData(long* frame_check, long* second_check, long* frames_per_second)
    {
        // Log timing
        (*frame_check)++;
        double seconds = std::chrono::duration<double, std::milli>(std::chrono::system_clock::now() - gobal_start_time).count() / 1000;
        while (seconds > (*second_check))
        {
            (*frames_per_second) = (*frame_check);
            (*frame_check) = 0;
            (*second_check) += 1;
        }
        // Draw the debug overlay
        std::cout.rdbuf(file_out.rdbuf());
        if (debug_complex)
        {
            std::cout << "DELTA TIME: " + std::to_string(DeltaTime()) + " ELAPSED SECONDS: " + std::to_string(seconds) + " ELAPSED FRAMES: " + std::to_string(elapsed_frames) + "\n"
                << "FRAME TIME: " + std::to_string(frame_time) + " FPS: " + std::to_string(*frames_per_second) + "\n";
        }
        else
        {
            std::cout << "FPS: " + std::to_string(*frames_per_second) + "\n";
        }

        // Set the fps variable
        current_fps = *frames_per_second;
    }

    // Sync frame step
    void SyncFrameStep()
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
    // void (*pre_loop_code)() : Custom global begin-step function
    // void (*post_loop_code)() : Custom global end-step function
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
            SyncFrameStep();
        }
    }
};

// -- General utility functions --

// Check if the vector contains the value
// std::vector<T> v : The vector to check
// T val : The value to check the presence of
template <typename T> bool
VectorContains(std::vector<T> v, T val) {
    return std::find(v.begin(), v.end(), val) != v.end();
}