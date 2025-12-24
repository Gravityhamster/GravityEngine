#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <iostream>
#include <chrono>
#include <math.h>
#include <future>
#include <thread>
#include <vector>
#include <string>
#include <unordered_map>
#include <random>

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

// Color struct (foreground and background)
// SDL_Color f : Letter color
// SDL_Color b : Background color
struct color
{
    SDL_Color f;
    SDL_Color b;
};

// Enum to define the current playback state of a sound channel
enum ChannelStates
{
    stopped,
    init,
    playing,
    paused,
    uninit
};

// Enum to define the type of sound currently playing on the channel
enum ChannelType
{
    file,
    synth
};

// Template for synth objects
class GravityEngine_Synth
{
public:
    float freq = 50.0f;
    int sample_frames;
    float* audio_data;
    float volume = 1;
    float pulse_width_delta = 0;

    // Some of this code provided by Copilot, unfortunately. 
    // I couldn't figure out how to feed the buffer in a way that didn't
    // make it sound like a demon was shrieking from the pits of hell.
    // In short, what copilot provided was a sinewave generator that handles proper data formatting
    // and phasing correction to prevent clicking. Then I took this code and encapsulated it into my
    // engine with a thread to ensure it always runs independently.
    // As such this is to be called from and audio channel ([game engine pointer]->BindSynthToChannel).
    // Please don't call this independently.
    // GravityEngine_Synth* synth : Synth object reference
    // SDL_AudioStream* stream : Audio stream that the synth audio plays on
    // SDL_AudioSpec* spec : Audio spec to format the audio with
    // SDL_AudioDeviceID dev : Device the audio will play on
    // ChannelStates state : Current state of the channel
    static void GenerateAudio(GravityEngine_Synth* synth, SDL_AudioStream* stream, SDL_AudioSpec* spec, SDL_AudioDeviceID dev, ChannelStates* state, bool* synth_playing)
    {
        SDL_GetAudioDeviceFormat(dev, spec, &synth->sample_frames);
        const float sample_rate = (float)spec->freq;

        // Use the device's preferred frame size
        int samples = synth->sample_frames * spec->channels;

        float* buffer = (float*)SDL_malloc(samples * sizeof(float));
        synth->audio_data = buffer;
        float phase = 0.0f;

        double q = 0.0;
        double v = synth->volume;

        while ((*state) == playing || (*state) == paused) {
            if ((*state) == paused)
                continue;

            for (int i = 0; i < samples; i++) {
                auto one = phase * 2.0f * 3.141592f;
                // buffer[i] = v*(sinf(one) > 0 ? 1 : -1); // SQUARE
                buffer[i] = v*(sinf(one) > ((sin(synth->pulse_width_delta)/2)*0.99)+0.5 ? 1 : -1); // PULSE
                // buffer[i] = v*sinf(one); // SINE
                // buffer[i] = v*fmod(one, 1); // SAWTOOTH
                phase += (synth->freq/2) / sample_rate;
                if (phase >= 1.0f) phase -= 1.0f;
            }

            SDL_PutAudioStreamData(stream, buffer, samples * sizeof(float));
            SDL_Delay(5);

            synth->pulse_width_delta += 1. / 20.;
        }

        SDL_free(buffer);
        (*synth_playing) = false;
    }
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

    // Gravity Engine private types
    private:

    // Gravity Engine private classes
    private:

        // Gravity Engine sound class
        class GravityEngine_Sound
        {
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
        std::vector<GravityEngine_AudioChannel*> audio_channels; // List of all audio channels
        std::vector<GravityEngine_Sound*> sounds; // List of all saved sounds
        int channels; // Channel count
        int mouse_wheel_state; // Store the current 

    // Gravity Engine Public Attributes
    public:
        bool debug_mode = false; // Show debug overlay
        bool debug_complex = false; // Show complex debug overlay

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
            if (cl == stat)
                return collision_static[y][x];
            else
                return collision_dynamic[y][x];
        }

        // Set the collision type at the given location
        // int x : Horizontal coordinate
        // int y : Vertical coordinate
        // col_layer cl : Layer to set collision on
        // int v : Collision type value
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

            // -= GAME END =-
            // Clenup goes here

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
        // int x : Horizontal position to place color
        // int y : Vertical position to place color
        // layer l : Layer to set color on
        // color col : Color struct instance
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
        // int x : Horizontal position to place character
        // int y : Vertical position to place character
        // layer l : Layer to place the character on
        // char c : Character to draw
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

        // Get character on layer
        // int x : Horizontal position to read character
        // int y : Vertical position to read character
        // layer l : Layer to read from
        char GetChar(int x, int y, layer l)
        {
            char c = NULL;

            // If the requested x and y is within the window...
            if (x >= 0 && x < canvas_w && y >= 0 && y < canvas_h)
            {
                // Set the character at these coordinates on the layer.
                if (l == debug) c = canvas_debug[y][x];
                if (l == ui) c = canvas_ui[y][x];
                if (l == background) c = canvas_bg[y][x];
                if (l == foreground) c = canvas_fg[y][x];
                if (l == entity) c = canvas_ent[y][x];
            }

            return c;
        }

        // Get color on layer
        // int x : Horizontal position to read color
        // int y : Vertical position to read color
        // layer l : Layer to read from
        color GetColor(int x, int y, layer l)
        {
            color c = {};

            // If the requested x and y is within the window...
            if (x >= 0 && x < canvas_w && y >= 0 && y < canvas_h)
            {
                // Set the character at these coordinates on the layer.
                if (l == debug) c = color_debug[y][x];
                if (l == ui) c = color_ui[y][x];
                if (l == background) c = color_bg[y][x];
                if (l == foreground) c = color_fg[y][x];
                if (l == entity) c = color_ent[y][x];
            }

            return c;
        }

        // Draw a line
        // int x : Starting horizontal point of the line
        // int y : Starting vertical point of the line
        // int to_x : Ending horizontal point of the line
        // int to_y : Ending vertical point of the line
        // layer l : Layer to draw on
        // color col : Color to draw with
        // char c : Character to draw with
        void DrawLine(int x, int y, int to_x, int to_y, GravityEngine_Core::layer l, color col, char c)
        {
            // Get the total length of the line
            double len = sqrt(((to_y - y) * (to_y - y)) + ((to_x - x) * (to_x - x)));
            // Get the individual dimensions of the line
            double len_x = to_x - x;
            double len_y = to_y - y;
            // Divide the individual length by the total length to get each step of the line
            double step_x = len_x / len;
            double step_y = len_y / len;
            // Define the stepping variables
            double xx = x;
            double yy = y;
            // Step through the line to create its segments
            while ( 
                    (len_x > 0 && floor(xx) <= to_x) ||
                    (len_y > 0 && floor(yy) <= to_y) ||
                    (len_x < 0 && floor(xx) >= to_x) ||
                    (len_y < 0 && floor(yy) >= to_y)
                )
            {
                DrawChar(round(xx), round(yy), l, c);
                DrawSetColor(round(xx), round(yy), l, col);
                xx += step_x;
                yy += step_y;
            }
        }

        // Change Font
        // string fpth : Path to the font file
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
        // int x : Horizontal position to read color
        // int y : Vertical position to read color
        // layer l : Layer to read from
        // string s : Text to draw to the screen
        // color col : Color struct instance
        void DrawTextString(int x, int y, layer l, std::string s, color col)
        {
            if (y >= canvas_h || y < 0)
                return;

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
        // int* ret_x : Pointer to store the horizontal position
        // int* ret_y : Pointer to store the vertical position
        void GetMousePosition(int* ret_x, int* ret_y)
        {
            float x, y;
            SDL_GetMouseState(&x, &y);

            *ret_x = floor((x / scr_w) * canvas_w);
            *ret_y = floor((y / scr_h) * canvas_h);
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
            return entity_list.size()-1;
        }

        // Remove the object from the entity list
        // GravityEngine_Object* object : Gravity engine managed object reference
        void RemoveObject(GravityEngine_Object* object)
        {
            std::erase(entity_list, object);
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

        // Draw character into the screen buffer with char c and color col
        // int x : Horizontal position to read color
        // int y : Vertical position to read color
        // char c : Character to insert into the screen buffer
        // color col : Color struct instance
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
                    DrawSetColor(q, i, entity, { {255,255,255}, {0,0,0} });
                    DrawSetColor(q, i, debug, { {255,255,255}, {0,0,0} });
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