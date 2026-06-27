#include "GravityEngineSDL.h"

GravityEngine_Core* geptr;
GravityEngine_Synth* synptr2;
bool s2isplaying = 0;
double global_timer = 1;
bool was = 0;
bool is = 0;
int cursor_x;
int cursor_y;
int channelcount = 64;
int rowcount = 0xffff;
int** songgrid; //[0xffff][64];
int bpm = 295; // Beats per minute
int fps = 60; // Frame rate in hz
int tps = 6; // Ticks per step
int ticknumber = 0;
double ticklength = 0;
int song_grid_h;
int song_grid_w;
bool running = false;
std::thread* timing_thread;

// Tracker colors
color primary_text_a = { {255, 255, 255}, {0, 0, 0} };
color header_text_a = { {255, 255, 255}, {0, 0, 100} };
color primary_text_b = { {0, 0, 0}, {255, 255, 255} };
color body_text_a = { {255, 255, 255}, {0, 0, 0} };
color body_text_b = { {0, 0, 0}, {255, 255, 255} };

// Song editor menu
enum menu
{
    song,
    chain,
    phrase,
    instrument
};

menu state = song;

// Object to handle all inputs
class input : public virtual GravityEngine_Object
{
    private:
        SDL_Scancode up = SDL_SCANCODE_UP;
        bool was_up = false;
        bool is_up = false;
        SDL_Scancode down = SDL_SCANCODE_DOWN;
        bool was_down = false;
        bool is_down = false;
        SDL_Scancode left = SDL_SCANCODE_LEFT;
        bool was_left = false;
        bool is_left = false;
        SDL_Scancode right = SDL_SCANCODE_RIGHT;
        bool was_right = false;
        bool is_right = false;

	public:
        input() {};
		~input() {};
		void begin_step() 
        {
            was_up = is_up;
            is_up = geptr->GetKeyState(up);
            was_down = is_down;
            is_down = geptr->GetKeyState(down);
            was_left = is_left;
            is_left = geptr->GetKeyState(left);
            was_right = is_right;
            is_right = geptr->GetKeyState(right);
        };
		void step() {};
		void end_step() {};

        bool is_up_pressed() { return is_up && !was_up; }
        bool is_up_down() { return is_up; }
        bool was_up_pressed() { return was_up; }

        bool is_down_pressed() { return is_down && !was_down; }
        bool is_down_down() { return is_down; }
        bool was_down_pressed() { return was_down; }

        bool is_left_pressed() { return is_left && !was_left; }
        bool is_left_down() { return is_left; }
        bool was_left_pressed() { return was_left; }

        bool is_right_pressed() { return is_right && !was_right; }
        bool is_right_down() { return is_right; }
        bool was_right_pressed() { return was_right; }
};

// Input getter
int inputgetter;

// Get note freq
double NoteFreq(int n)
{
    // https://superglobalcalculator.com/calculators/music/piano-key-frequency/
    return 440.0 * pow(2.0, (n - 49.0) / 12.0);
}

// Beats-per-minute to Tick length in nanoseconds
// b : bpm
double BpmToTicklength(int b)
{
    // 295 BPM * 6 TPS * 4 Steps per Beat = 7080 ticks ber minute
    // 7080 ticks per minute / 60 seconds = 118 ticks per second
    // = 1 tick every 1/118th of a second
    // TickLength = 1000000000 / 118 nano seconds
    return 1000000000 / ((b * tps * 4) / 60);
}

// Execute tick
void DoTick()
{
    // Offstep tick
    if (ticknumber % tps == 0)
    {
        synptr2->freq = NoteFreq(40);
        synptr2->volume = 0.25;
    }
    else
    {
    }
    ticknumber++;
}

// Find string f in s
bool str_contains(std::string s, std::string f) { return s.find(f) != std::string::npos; }

// Convert i to hex string
std::string IntToHexString(int i)
{
    // Write the int to a stream as hex
    std::ostringstream hexs;
    hexs << std::hex << i;

    // Get the string from the stream
    std::string tempstr = hexs.str();
    std::string upperstr = "";

    // https://www.geeksforgeeks.org/cpp/toupper-in-cpp/
    // To upper
    for (auto x : tempstr)
        upperstr += (char)toupper(x);

    // Return
    return upperstr;
}

// Draw Song Editor UI
// off_x : UI offset on the x axis
// off_y : UI offset on the y axis
// type : Draw type (What do you want to redraw?) [all, title, x, y, navigator]
void DrawSongUI(int off_x, int off_y, std::string type)
{
    // Width and height of the screen
    int h = song_grid_h;
    int w = song_grid_w;

    // Menu title
    if (str_contains(type, "all") || str_contains(type, "title"))
    {
        geptr->DrawTextString(0, 1, geptr->background, "SONG", primary_text_a);
    }

    // Row numbers
    if (str_contains(type, "all") || str_contains(type, "y"))
    {
        // Draw the row numbers
        for (int i = 0; i < h && i + off_y < rowcount; i++)
        {
            int tempint = i + off_y;

            auto upperstr = IntToHexString(tempint);

            upperstr.insert(upperstr.begin(), 4 - upperstr.size(), '0');
            geptr->DrawTextString(0, 3 + i, geptr->background, upperstr, primary_text_a);
        }
    }

    // Channel headers
    if (str_contains(type, "all") || str_contains(type, "x"))
    {
        // Draw the headers
        for (int i = 0; i < w && i + off_x < channelcount; i++)
        {
            std::string tempstr = std::to_string(i + off_x);
            tempstr.insert(tempstr.begin(), 2 - tempstr.size(), '0');
            geptr->DrawTextString(5 + i*5, 2, geptr->background, "CH" + tempstr, header_text_a);
        }
    }

    // Song grid navigator
    if (str_contains(type, "all") || str_contains(type, "navigator"))
    {
        //Draw the song grid
        for (int y = 0; y < h && y + off_y < rowcount; y++)
        {
            for (int x = 0; x < w && x + off_x < channelcount; x++)
            {
                int chain = songgrid[y + off_y][x + off_x];
                if (chain == -1)
                {
                    geptr->DrawTextString(5 + x * 5, 3 + y, geptr->background, "----", primary_text_a);
                }
                else
                {
                    auto outstr = IntToHexString(chain);
                    outstr.insert(outstr.begin(), 4 - outstr.size(), '0');
                    geptr->DrawTextString(5 + x * 5, 3 + y, geptr->background, outstr, primary_text_a);
                }
            }
        }
    }
}

// Handle tick hitting - This should be called from a separate thread
void TrackTicks()
{
    // The next time is defined by starting at the current time
    auto next = std::chrono::steady_clock::now();

    while (running == true)
    {
        // Execute tick
        DoTick();

        // Sync timing
        next += std::chrono::nanoseconds((int64_t)ticklength);
        while (std::chrono::steady_clock::now() < next) { /* Spin in place until the clock hits the next tick */ }
    }
}

// Master pre code
void GameInit()
{
    // Set song grid w and h
    song_grid_h = geptr->GetCanvasH() - 4;
    song_grid_w = (geptr->GetCanvasW() - 5) / 5;

    // Init song phrase list
    songgrid = new int* [rowcount];
    for (int i = 0; i < rowcount; i++)
        songgrid[i] = new int[channelcount];
    for (int y = 0; y < rowcount; y++)
    {
        for (int x = 0; x < channelcount; x++)
        {
            songgrid[y][x] = -1;
        }
    }

    // Start song UI
    DrawSongUI(0, 0, "all");

    // Test: Init synth and play it
    synptr2 = new GravityEngine_Synth();
    synptr2->pulse_width_freq = 0.5f;
    synptr2->panning = 0.0f;
    synptr2->freq = 261.63;
    synptr2->volume = 0;
    synptr2->volume_freq = -10;
    synptr2->waveform = triangle;
    geptr->BindSynthToChannel(synptr2, 0);

    // Test: Init file play and play it
    int i = geptr->AddSound("DrumBeat.wav");

    // Add the input check object
    inputgetter = geptr->AddObject(new input());

    // Get the length of the tick
    ticklength = BpmToTicklength(bpm);

    // Start tick tracker
    running = true;
    std::thread tt(TrackTicks);
    tt.detach();
    timing_thread = &tt;
}

// Master pre code
void PreGameLoop()
{
    if (geptr->GetKeyState(SDL_SCANCODE_SPACE))
    {
        if (geptr->GetChannelState(1) == stopped)
        {
            geptr->PlaySoundOnChannel(0, 1, true);
        }
    }
    else
    {
        geptr->StopChannel(1);
    }
}

// Master post code
void PostGameLoop()
{
    // Handle input for the song menu
    if (state == song)
    {
        int wcx = cursor_x;
        int wcy = cursor_y;

        if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_right_pressed())
            cursor_x++;
        if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_left_pressed())
            cursor_x--;
        if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_up_pressed())
            cursor_y--;
        if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_down_pressed())
            cursor_y++;

        cursor_x = SDL_clamp(cursor_x, 0, channelcount);
        cursor_y = SDL_clamp(cursor_y, 0, rowcount);

        if (cursor_x != wcx || cursor_y != wcy)
            DrawSongUI(cursor_x, cursor_y, "all");
    }
}

int main()
{
    // Init engine - 128x72 is generally the largest you can get and still maintain good performance
    GravityEngine_Core ge_inst = GravityEngine_Core("Game", "com.example.game", "1.0", 96/2, 54/2, fps, 1920, 1080, "./GameFont.ttf", channelcount);

    ge_inst.debug_mode = true; // Show debug overlay
    ge_inst.debug_complex = false; // Show all information
    geptr = &ge_inst; // Set the pointer to the console engine class

    // Start game loop
    ge_inst.Start(&GameInit, &PreGameLoop, &PostGameLoop);

    // Cleanup all dynamically allocated data
    delete[] songgrid;
    running = false;

    // Report success to host
    return 0;
}