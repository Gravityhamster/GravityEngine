#include "GravityEngineSDL.h"

GravityEngine_Core* geptr;
GravityEngine_Synth* synptr2;

// Global variables --
int ticknumber = 0; // Track tick progress
int cursor_x; // X location of the user's cursor
int cursor_y; // Y location of the user's cursor
int offset_x; // X offset of the editor scroll
int offset_y; // Y offset of the editor scroll
const int channelcount = 64; // How many audio channels in the song
int rowcount = 0xffff; // How many rows in the song - 65535 chains * 16 phrases * 16 steps = 16776960 steps / 4 steps = 4194240 beats
int bpm = 170; // Beats per minute of the song
int tps = 6; // Ticks per step of the song
int fps = 60; // Frame rate in hz of the UI
double ticklength = 0; // Nanoseconds per tick
int song_grid_h; // Height of the Song Editor UI
int song_grid_w; // Width of the Song Editor UI
bool running = false; // Is the song currently playing?
std::thread* timing_thread; // Thread to play ticks

// Tracker object concepts --

// Chains - Lists of phrases
class chain 
{ 
    public:
        static const int length = 16;
        int arr[length]; // 1w x 16h - List of phrases

        // Create chain
        chain() 
        {
            // Fill the chain with blanks
            for (int x = 0; x < length; x++)
                arr[x] = -1;
        };

        // Destruct chain
        ~chain() {};
};

// Phrases - Lists of notes (i.e. 4/4 Measures)
class phrase 
{
    public:
        static const int len_x = 8;
        static const int len_y = 16;
        int arr[len_y][len_x]; // 8w x 16h - List of notes

        // Create pharse
        phrase()
        {
            // Fill the chain with blanks
            for (int y = 0; y < len_y; y++)
                for (int x = 0; x < len_x; x++)
                    arr[y][x] = -1;
        };

        // Destruct phrase
        ~phrase() {};
}; 

// Instruments - Note audio definitions
class instrument 
{
    public:
        // Audio parameters
        ChannelType type;
};

// Tables - List of modulations for the currently playing instrument
class table 
{ 
    public:
        static const int len_x = 7;
        static const int len_y = 16;
        int arr[len_y][len_x]; // 7w x 16h - List of ticks for sound automation 

        // Create pharse
        table()
        {
            // Fill the chain with blanks
            for (int y = 0; y < len_y; y++)
                for (int x = 0; x < len_x; x++)
                    arr[y][x] = -1;
        };

        // Destruct phrase
        ~table() {};
};

// ChannelSequencer - Track position of the channel in time in the song
class channelsequencer
{
    public:
        int channelnumber = 0;
        int chain_ptr = 0; // Int position of the channel in the song
        int phrase_ptr = 0; // Int position of the channel in the chain
        int step_ptr = 0; // Int position of the channel in the phrase
        int tick_ptr = 0; // Int position of the channel in the table

        void sub_step()
        {
            tick_ptr++;
            tick_ptr = tick_ptr % table::len_y;
        }

        void step()
        {
            step_ptr++;
            if (step_ptr % phrase::len_y == 0)
            {
                step_ptr = 0;
                inc_phrase();
            }
        }

    private:

        void inc_phrase()
        {
            phrase_ptr++;
            if (phrase_ptr % chain::length == 0)
            {
                phrase_ptr = 0;
                inc_chain();
            }
        }

        void inc_chain()
        {
            chain_ptr++;
        }
};

// Data structures --
int** songgrid; //[0xffff][64];
std::vector<chain*> chainlist;
std::vector<phrase*> phraselist;
std::vector<instrument*> instrumentlist;
std::vector<table*> tablelist;
channelsequencer channellist[channelcount];

// Tracker colors --
color primary_text_a = { {255, 255, 255}, {0, 0, 0} };
color header_text_a = { {255, 255, 255}, {0, 0, 100} };
color primary_text_b = { {0, 0, 0}, {255, 255, 255} };
color body_text_a = { {255, 255, 255}, {0, 0, 0} };
color body_text_b = { {0, 0, 0}, {255, 255, 255} };

// Song editor menu
enum menu
{
    m_song,
    m_chain,
    m_phrase,
    m_instrument
};

menu state = m_song;

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
// ------------
// This is the tick loop; All song execution should go inside this function
// Table>Phrase>Chain>Song <- Per channel
void DoTick()
{
    if (ticknumber % tps == 0)
    {
        // Do Step Code --
        synptr2->freq = NoteFreq(48);
        synptr2->volume = 1;

        // Step the channel sequencers
        for (int i = 0; i < channelcount; i++)
            channellist[i].step();
    }
    
    // Do Sub-step Code --

    // Tick the channel sequencers
    for (int i = 0; i < channelcount; i++)
        channellist[i].sub_step();

    // Debug : Draw channel 0's sequence
    geptr->DrawTextString(10, 0, geptr->background,
        std::to_string(channellist[0].chain_ptr) + " - " +
        std::to_string(channellist[0].phrase_ptr) + " - " +
        std::to_string(channellist[0].step_ptr) + " - " +
        std::to_string(channellist[0].tick_ptr) + "    ",
        primary_text_a);

    // Increment global song position in ticks --
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
        while (true) 
        { 
            // Get the sleep time remaining
            auto rem = next - std::chrono::steady_clock::now();

            // Break if we have no more time
            if (rem <= std::chrono::nanoseconds(0))
                break;

            // Should we sleep or nah?
            if (rem > std::chrono::milliseconds(5))
                SDL_Delay(1); // Sleep the thread to relieve the CPU
            else
            {
                /* Spin in place until the clock hits the next frame */
            }        
        }
    }
}

// Master pre code
void GameInit()
{
    // Set song grid w and h
    song_grid_h = geptr->GetCanvasH() - 4;
    song_grid_w = (geptr->GetCanvasW() - 5) / 5;

    // Init channel sequencers
    for (int i = 0; i < channelcount; i++)
        channellist->channelnumber = i;

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
    synptr2->panning = 0.5f;
    synptr2->freq = 261.63;
    synptr2->volume = 0;
    synptr2->volume_freq = -50;
    synptr2->waveform = triangle;
    geptr->BindSynthToChannel(synptr2, 0);

    // Test: Init file play and play it
    int i = geptr->AddSound("DrumBeat.wav");
    geptr->PlaySoundOnChannel(0, 1, true);

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
}

// Master post code
void PostGameLoop()
{
    // Handle input for the song menu
    if (state == m_song)
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