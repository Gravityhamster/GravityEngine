#include "GravityEngineSDL.h"

GravityEngine_Core* geptr;
GravityEngine_Synth* synptr2;

// Editor held modifier
enum edit_mod
{
    left,
    right,
    center
};

// Global variables --
int ticknumber = 0; // Track tick progress
int cursor_x; // X location of the user's cursor
int cursor_y; // Y location of the user's cursor
int offset_x; // X offset of the editor scroll
int offset_y; // Y offset of the editor scroll
int chain_offset_y; // Y offset when editing the chain
int inputholdtimer = 0; // The timer for checking if an input should be considered held-down
int inputholdthreshold = 15; // Frames til in input should start repeating
int inputholddelay = 2; // How many frames to skip on hold (2 == every other, 3 == every other 3, etc.) 
const int channelcount = 64; // How many audio channels in the song
int rowcount = 0xffff; // How many rows in the song - 65535 chains * 16 phrases * 16 steps = 16776960 steps / 4 steps = 4194240 beats
int bpm = 170; // Beats per minute of the song
int tps = 6; // Ticks per step of the song
int fps = 60; // Frame rate in hz of the UI
double ticklength = 0; // Nanoseconds per tick
int song_grid_h; // Height of the Song Editor UI
int song_grid_w; // Width of the Song Editor UI
int chain_grid_h; // Height of the Chain Editor UI
int chain_grid_w; // Width of the Chain Editor UI
bool running = false; // Is the song currently playing?
std::thread* timing_thread; // Thread to play ticks
edit_mod leftrightcenter = center;
int copied_chain = -1;
int copied_phrase = -1;
int open_chain = -1;

// Find string f in s
bool str_contains(std::string s, std::string f) { return s.find(f) != std::string::npos; }

// Remove char from string
std::string str_remove(std::string s, char c)
{
    // https://www.geeksforgeeks.org/dsa/remove-all-occurrences-of-a-character-in-a-string/
    int j = 0; // Size variable
    for (int i = 0; i < s.size(); i++)
    {
        // Move chars to keep down to the front
        if (s[i] != c)
            s[j++] = s[i];
    }
    // Truncate to size
    s.resize(j);
    return s;
}

// Tracker object concepts --

// Chains - Lists of phrases
class chain 
{ 
    public:
        static const int length = 16;
        int arr[length]; // 1w x 16h - List of phrases
        int arr_transpose[length]; // 1w x 16h - List of phrases

        // Create chain
        chain() 
        {
            // Fill the chain with blanks
            for (int x = 0; x < length; x++)
                arr[x] = -1;
            for (int x = 0; x < length; x++)
                arr_transpose[x] = 0x0000;
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
        int channelnumber = 0; // Which channel is this sequencer assigned to?
        int chain_ptr = 0; // Int position of the channel in the song
        int phrase_ptr = 0; // Int position of the channel in the chain
        int step_ptr = 0; // Int position of the channel in the phrase
        int tick_ptr = 0; // Int position of the channel in the table

        // Count tick
        void sub_step()
        {
            // Increment tick in table
            tick_ptr++;
            // Go back to the 0th tick if we have reached the end of the table
            tick_ptr = tick_ptr % table::len_y;
        }

        // Count step
        void step()
        {
            // Increment step in phrase
            step_ptr++;
            // If the step ptr has reached the end of the phrase, go back to the 0th step
            if (step_ptr % phrase::len_y == 0)
            {
                step_ptr = 0;
                // Increment the phrase pointer in the chain
                inc_phrase();
            }
        }

    private:

        void inc_phrase()
        {
            // Increment phrase in chain
            phrase_ptr++;
            if (phrase_ptr % chain::length == 0)
            {
                phrase_ptr = 0;
                // Increment the chain pointer in the song
                inc_chain();
            }
        }

        void inc_chain()
        {
            // Increment chain in song
            chain_ptr++;
            // If the chain ptr has reached the end of the song, go back to the 0th chain
            if (chain_ptr % rowcount == 0)
            {
                chain_ptr = 0;
            }
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
    public:

        // Keycodes for inputs --

        // Keycode for Up button
        const SDL_Scancode up = SDL_SCANCODE_UP;

        // Keycode for Down button
        const SDL_Scancode down = SDL_SCANCODE_DOWN;

        // Keycode for Left button
        const SDL_Scancode left = SDL_SCANCODE_LEFT;

        // Keycode for Right button
        const SDL_Scancode right = SDL_SCANCODE_RIGHT;

        // Key code for A button
        const SDL_Scancode a = SDL_SCANCODE_Z;

        // Key code for B button
        const SDL_Scancode b = SDL_SCANCODE_X;

        // Key code for Start button
        const SDL_Scancode start = SDL_SCANCODE_RETURN;

        // Key code for Select button
        const SDL_Scancode select = SDL_SCANCODE_SPACE;

        // Key code for Shift button
        const SDL_Scancode shift = SDL_SCANCODE_LSHIFT;

        // Get the last keycode pressed
        SDL_Scancode last_pressed = SDL_SCANCODE_UNKNOWN;
        bool doubleclick = false;
        std::string presscode = "0";
        std::string lastpresscode = "0";

    private:

        // State variables for keys
        bool was_up = false; // Button was pressed last frame
        bool is_up = false; // Button is pressed this frame

        bool was_down = false; // Button was pressed last frame
        bool is_down = false; // Button is pressed this frame

        bool was_left = false; // Button was pressed last frame
        bool is_left = false; // Button is pressed this frame

        bool was_right = false; // Button was pressed last frame
        bool is_right = false; // Button is pressed this frame

        bool was_a = false; // Button was pressed last frame
        bool is_a = false; // Button is pressed this frame

        bool was_b = false; // Button was pressed last frame
        bool is_b = false; // Button is pressed this frame

        bool was_start = false; // Button was pressed last frame
        bool is_start = false; // Button is pressed this frame

        bool was_select = false; // Button was pressed last frame
        bool is_select = false; // Button is pressed this frame

        bool was_shift = false; // Button was pressed last frame
        bool is_shift = false; // Button is pressed this frame

	public:
        input() {};
		~input() {};
		void begin_step() 
        {
            was_up = is_up; // Save last frame
            is_up = geptr->GetKeyState(up); // Get this frame
            was_down = is_down; // Save last frame
            is_down = geptr->GetKeyState(down); // Get this frame
            was_left = is_left; // Save last frame
            is_left = geptr->GetKeyState(left); // Get this frame
            was_right = is_right; // Save last frame
            is_right = geptr->GetKeyState(right); // Get this frame
            was_a = is_a; // Save last frame
            is_a = geptr->GetKeyState(a); // Get this frame
            was_b = is_b; // Save last frame
            is_b = geptr->GetKeyState(b); // Get this frame
            was_start = is_start; // Save last frame
            is_start = geptr->GetKeyState(start); // Get this frame
            was_select = is_select; // Save last frame
            is_select = geptr->GetKeyState(select); // Get this frame
            was_shift = is_shift; // Save last frame
            is_shift = geptr->GetKeyState(shift); // Get this frame

            // Handle all double taps
            std::string temppresscode = 
                std::to_string(is_up && !was_up) +
                std::to_string(is_down && !was_down) +
                std::to_string(is_left && !was_left) +
                std::to_string(is_right && !was_right) +
                std::to_string(is_a && !was_a) +
                std::to_string(is_b && !was_b) +
                std::to_string(is_start && !was_start) +
                std::to_string(is_select && !was_select) +
                std::to_string(is_shift && !was_shift);
            // Keep a history of inputs
            if (std::stoi(temppresscode) != 0)
            {
                lastpresscode = presscode;
                presscode = temppresscode;
            }
            temppresscode = presscode;
            // Truncate the presscode down to 1s if the value contains 1s
            if (str_contains(temppresscode, "1")) temppresscode = str_remove(temppresscode, '0');
            // If the history is equal and the total number of inputs == 1, then a doubleclick has occured
            if (lastpresscode == presscode && std::stoi(temppresscode) == 1)
                doubleclick = true;
            else
                doubleclick = false;
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

        bool is_a_pressed() { return is_a && !was_a; }
        bool is_a_down() { return is_a; }
        bool was_a_pressed() { return was_a; }

        bool is_b_pressed() { return is_b && !was_b; }
        bool is_b_down() { return is_b; }
        bool was_b_pressed() { return was_b; }

        bool is_start_pressed() { return is_start && !was_start; }
        bool is_start_down() { return is_start; }
        bool was_start_pressed() { return was_start; }

        bool is_select_pressed() { return is_select && !was_select; }
        bool is_select_down() { return is_select; }
        bool was_select_pressed() { return was_select; }

        bool is_shift_pressed() { return is_shift && !was_shift; }
        bool is_shift_down() { return is_shift; }
        bool was_shift_pressed() { return was_shift; }
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
    geptr->DrawTextString(10, 0, geptr->entity,
        std::to_string(channellist[0].chain_ptr) + " - " +
        std::to_string(channellist[0].phrase_ptr) + " - " +
        std::to_string(channellist[0].step_ptr) + " - " +
        std::to_string(channellist[0].tick_ptr) + "    ",
        primary_text_a);

    // Increment global song position in ticks --
    ticknumber++;
}

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
        geptr->DrawTextString(0, 1, geptr->entity, "SONG", primary_text_a);
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
            geptr->DrawTextString(0, 3 + i, geptr->entity, upperstr, primary_text_a);
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
            geptr->DrawTextString(5 + i*5, 2, geptr->entity, "CH" + tempstr, header_text_a);
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
                // Is the cursor currently hovering this cell? Set color accordingly
                color thiscolor = (x == cursor_x && y == cursor_y ? primary_text_b : primary_text_a);
                // Get the current song grid value
                int chain = songgrid[y + off_y][x + off_x];
                if (chain == -1)
                {
                    // Draw null chain
                    geptr->DrawTextString(5 + x * 5, 3 + y, geptr->entity, "----", thiscolor);
                }
                else
                {
                    // Draw chain number
                    auto outstr = IntToHexString(chain);
                    outstr.insert(outstr.begin(), 4 - outstr.size(), '0');
                    geptr->DrawTextString(5 + x * 5, 3 + y, geptr->entity, outstr, thiscolor);
                }

                // Is a modifier key held on the selected cell?
                if (x == cursor_x && y == cursor_y)
                {
                    // Modify right part of the number
                    if (leftrightcenter == left)
                    {
                        geptr->DrawSetColor(5 + x * 5, 3 + y, geptr->entity, primary_text_a);
                        geptr->DrawSetColor(5 + x * 5 + 1, 3 + y, geptr->entity, primary_text_a);
                    }
                    // Modify left part of the number
                    else if (leftrightcenter == right)
                    {
                        geptr->DrawSetColor(5 + x * 5 + 2, 3 + y, geptr->entity, primary_text_a);
                        geptr->DrawSetColor(5 + x * 5 + 3, 3 + y, geptr->entity, primary_text_a);
                    }
                }
            }
        }
    }
}

// Draw Song Editor UI
// off_y : UI offset on the y axis
// type : Draw type (What do you want to redraw?) [all, title, x, y, navigator]
void DrawChainUI(int off_y, std::string type)
{
    // TODO: Should we make offsety?

    // Width and height of the screen
    int h = chain_grid_h;
    int w = 1;

    // Menu title
    if (str_contains(type, "all") || str_contains(type, "title"))
    {
        // Draw chain number
        auto outstr = IntToHexString(open_chain);
        outstr.insert(outstr.begin(), 4 - outstr.size(), '0');
        geptr->DrawTextString(0, 1, geptr->entity, "CHAIN - " + outstr, primary_text_a);
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
            geptr->DrawTextString(0, 3 + i, geptr->entity, upperstr, primary_text_a);
        }
    }

    // Channel headers
    if (str_contains(type, "all") || str_contains(type, "x"))
    {
        // Draw the headers
        geptr->DrawTextString(5, 2, geptr->entity, "PHSE", header_text_a);
        geptr->DrawTextString(10, 2, geptr->entity, "TRPS", header_text_a);
    }

    // Song grid navigator
    if (str_contains(type, "all") || str_contains(type, "navigator"))
    {
        //Draw the song grid
        for (int y = 0; y < h && y + off_y < rowcount; y++)
        {
            // Is the cursor currently hovering this cell? Set color accordingly
            color thiscolor = (0 == cursor_x && y == cursor_y ? primary_text_b : primary_text_a);
            color thiscolor_t = (1 == cursor_x && y == cursor_y ? primary_text_b : primary_text_a);

            // Get the current chain grid value
            int chain = chainlist[open_chain]->arr[y + off_y];
            if (chain == -1)
            {
                // Draw null chain
                geptr->DrawTextString(5, 3 + y, geptr->entity, "----", thiscolor);
            }
            else
            {
                // Draw chain number
                auto outstr = IntToHexString(chain);
                outstr.insert(outstr.begin(), 4 - outstr.size(), '0');
                geptr->DrawTextString(5, 3 + y, geptr->entity, outstr, thiscolor);
            }

            // Draw chain transposition
            int chain_trsp = chainlist[open_chain]->arr_transpose[y + off_y];
            auto outstr = IntToHexString(chain_trsp);
            outstr.insert(outstr.begin(), 4 - outstr.size(), '0');
            geptr->DrawTextString(10, 3 + y, geptr->entity, outstr, thiscolor_t);

            // Is a modifier key held on the selected cell?
            if (y == cursor_y)
            {
                int trsp_offset = 5*(1 == cursor_x);
                // Modify right part of the number
                if (leftrightcenter == left)
                {
                    geptr->DrawSetColor(5 + trsp_offset, 3 + y, geptr->entity, primary_text_a);
                    geptr->DrawSetColor(6 + trsp_offset, 3 + y, geptr->entity, primary_text_a);
                }
                // Modify left part of the number
                else if (leftrightcenter == right)
                {
                    geptr->DrawSetColor(7 + trsp_offset, 3 + y, geptr->entity, primary_text_a);
                    geptr->DrawSetColor(8 + trsp_offset, 3 + y, geptr->entity, primary_text_a);
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

// Handle repeating movements
void HandleMovementRepeaters()
{
    // Reset movement repeaters
    if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_right_pressed())
        inputholdtimer = 0;
    else if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_left_pressed())
        inputholdtimer = 0;
    else if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_up_pressed())
        inputholdtimer = 0;
    else if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_down_pressed())
        inputholdtimer = 0;

    // Increment hold timer
    if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_right_down())
        inputholdtimer++;
    else if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_left_down())
        inputholdtimer++;
    else if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_up_down())
        inputholdtimer++;
    else if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_down_down())
        inputholdtimer++;
    else
        inputholdtimer = 0;
}

// Insert at arbitrary location
template <typename T> void
InsertAt(std::vector<T*>* vec, int index, T* ptr)
{
    // Fill with nulls up to index
    for (int i = 0; i < index; i++)
    {
        if (vec->size() <= i)
            vec->insert(vec->begin() + i, nullptr);
    }

    // Insert pointer at index
    vec->insert(vec->begin() + index, ptr);
}

// Get at arbitrary location
template <typename T> T*
GetAt(std::vector<T*>* vec, int index)
{
    // Does this index exist yet in the vec
    if (vec->size() <= index)
        return nullptr;
    // Yes? Then return the value 
    else
        return (*vec)[index];
}

// Get next empty index
template <typename T> int
GetNextEmpty(std::vector<T*>* vec)
{
    int index = 0;
    // Iterate through, trying to find the next null or the end of the vector
    while (index < vec->size())
    {
        // If this is an empty cell, return this index
        if ((*vec)[index] == nullptr)
            return index;
        // Inc index
        index++;
    }
    // Returnt he last index
    return index;
}

// Handle movement
void EditorControl()
{
    // Set checker variables
    bool breakend = false;
    int goright = 0;
    int goleft = 0;
    int goup = 0;
    int godown = 0;

    // Move the cursor
    if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_right_pressed() ||
        (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_right_down() &&
            (inputholdtimer > inputholdthreshold && inputholdtimer % inputholddelay == 0)))
        goright = 1;
    if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_left_pressed() ||
        (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_left_down() &&
            (inputholdtimer > inputholdthreshold && inputholdtimer % inputholddelay == 0)))
        goleft = 1;
    if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_up_pressed() ||
        (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_up_down() &&
            (inputholdtimer > inputholdthreshold && inputholdtimer % inputholddelay == 0)))
        goup = 1;
    if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_down_pressed() ||
        (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_down_down() &&
            (inputholdtimer > inputholdthreshold && inputholdtimer % inputholddelay == 0)))
        godown = 1;

    // Edit part
    if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_a_down() &&
        dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_shift_down())
        leftrightcenter = right;
    else if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_a_down())
        leftrightcenter = left;
    else
        leftrightcenter = center;

    // Handle input for the song menu
    if (state == m_song && !breakend)
    {
        // Modify value
        if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_a_pressed())
        {
            // If the songgrid value is unfilled, insert 0
            if (songgrid[cursor_y + offset_y][cursor_x + offset_x] == -1)
            {
                if (copied_chain == -1)
                {
                    // Set UI reference to Hex0
                    songgrid[cursor_y + offset_y][cursor_x + offset_x] = 0x0000;
                    // If this chain doesn't exist yet, insert it
                    if (GetAt(&chainlist, songgrid[cursor_y + offset_y][cursor_x + offset_x]) == nullptr)
                        InsertAt(&chainlist, songgrid[cursor_y + offset_y][cursor_x + offset_x], new chain());
                }
                else
                {
                    // Set UI reference to copied chain
                    songgrid[cursor_y + offset_y][cursor_x + offset_x] = copied_chain;
                }
            }
            // If double click, add a new chain
            else if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->doubleclick)
            {
                // Set UI reference to the next empty
                songgrid[cursor_y + offset_y][cursor_x + offset_x] = GetNextEmpty(&chainlist);
                // Add the new chain
                InsertAt(&chainlist, songgrid[cursor_y + offset_y][cursor_x + offset_x], new chain());
                // Copy to clipboard
                copied_chain = songgrid[cursor_y + offset_y][cursor_x + offset_x];
            }
            else
            {
                // Copy to clipboard
                copied_chain = songgrid[cursor_y + offset_y][cursor_x + offset_x];
            }
        }

        // Do actions given the context --

        // Editing
        if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_a_down())
        {
            // Movement keys
            if (goup || godown || goright || goleft)
            {
                // Set to 0 if it isn't set
                if (songgrid[cursor_y + offset_y][cursor_x + offset_x] == -1)
                    songgrid[cursor_y + offset_y][cursor_x + offset_x] = 0x0000;

                // Mod the left two digits
                if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_shift_down())
                {
                    if (goup) songgrid[cursor_y + offset_y][cursor_x + offset_x] += 0x1000;
                    if (godown) songgrid[cursor_y + offset_y][cursor_x + offset_x] -= 0x1000;
                    if (goright) songgrid[cursor_y + offset_y][cursor_x + offset_x] += 0x0100;
                    if (goleft) songgrid[cursor_y + offset_y][cursor_x + offset_x] -= 0x0100;
                }
                // Mod the right two digits
                else
                {
                    if (goup) songgrid[cursor_y + offset_y][cursor_x + offset_x] += 0x0010;
                    if (godown) songgrid[cursor_y + offset_y][cursor_x + offset_x] -= 0x0010;
                    if (goright) songgrid[cursor_y + offset_y][cursor_x + offset_x] += 0x0001;
                    if (goleft) songgrid[cursor_y + offset_y][cursor_x + offset_x] -= 0x0001;
                }

                // Wrap the cell between 0x0000 and 0xFFFF
                if (songgrid[cursor_y + offset_y][cursor_x + offset_x] < 0)
                    songgrid[cursor_y + offset_y][cursor_x + offset_x] = 0xFFFF + (songgrid[cursor_y + offset_y][cursor_x + offset_x] + 1);
                if (songgrid[cursor_y + offset_y][cursor_x + offset_x] > 0xFFFF)
                    songgrid[cursor_y + offset_y][cursor_x + offset_x] = (songgrid[cursor_y + offset_y][cursor_x + offset_x] - 1) - 0xFFFF;

                // Copy to clipboard
                copied_chain = songgrid[cursor_y + offset_y][cursor_x + offset_x];
            }

            // Handle deletes
            if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_b_pressed())
                songgrid[cursor_y + offset_y][cursor_x + offset_x] = -1;
        }
        // Goto Chain Editor
        else if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_select_down())
        {
            // Go to the next page over
            if (goright)
            {
                // Set open chain
                if (songgrid[cursor_y + offset_y][cursor_x + offset_x] != -1)
                    open_chain = songgrid[cursor_y + offset_y][cursor_x + offset_x];
                // If the open_chain is valid
                if (open_chain != -1)
                {
                    // Check if the chain does not exist
                    if (GetAt(&chainlist, open_chain) == nullptr)
                    {
                        InsertAt(&chainlist, open_chain, new chain());
                    }
                    // Chain
                    state = m_chain;
                    cursor_x = SDL_clamp(cursor_x, 0, 1);
                    cursor_y = SDL_clamp(cursor_y, 0, chain::length-1);
                    breakend = true;
                }
            }
        }
        // Moving
        else
        {
            cursor_x += goright - goleft;
            cursor_y += godown - goup;
        }

        // Move the page
        if (cursor_y > song_grid_h - 1)
            offset_y += 1;
        if (cursor_x > song_grid_w - 1)
            offset_x += 1;
        if (cursor_y < 0)
            offset_y -= 1;
        if (cursor_x < 0)
            offset_x -= 1;

        // Clamp the cursor and offsets
        cursor_x = SDL_clamp(cursor_x, 0, song_grid_w-1);
        cursor_y = SDL_clamp(cursor_y, 0, song_grid_h-1);
        offset_x = SDL_clamp(offset_x, 0, channelcount-song_grid_w);
        offset_y = SDL_clamp(offset_y, 0, rowcount-song_grid_h);

        // Do we want to update the UI?
        DrawSongUI(offset_x, offset_y, "all");
    }

    // Handle input for the chain menu
    if (state == m_chain && !breakend)
    {
        // Modify value
        if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_a_pressed())
        {
            // Edit actual phrase
            if (cursor_x == 0)
            {
                // If the chainlist value is unfilled, insert 0
                if (chainlist[open_chain]->arr[cursor_y + chain_offset_y] == -1)
                {
                    if (copied_chain == -1)
                    {
                        // Set UI reference to Hex0
                        chainlist[open_chain]->arr[cursor_y + chain_offset_y] = 0x0000;
                        // If this chain doesn't exist yet, insert it
                        if (GetAt(&phraselist, chainlist[open_chain]->arr[cursor_y + chain_offset_y]) == nullptr)
                            InsertAt(&phraselist, chainlist[open_chain]->arr[cursor_y + chain_offset_y], new phrase());
                    }
                    else
                    {
                        // Set UI reference to copied chain
                        chainlist[open_chain]->arr[cursor_y + chain_offset_y] = copied_chain;
                    }
                }
                // If double click, add a new chain
                else if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->doubleclick)
                {
                    // Set UI reference to the next empty
                    chainlist[open_chain]->arr[cursor_y + chain_offset_y] = GetNextEmpty(&phraselist);
                    // Add the new chain
                    InsertAt(&phraselist, chainlist[open_chain]->arr[cursor_y + chain_offset_y], new phrase());
                    // Copy to clipboard
                    copied_chain = chainlist[open_chain]->arr[cursor_y + chain_offset_y];
                }
                else
                {
                    // Copy to clipboard
                    copied_chain = chainlist[open_chain]->arr[cursor_y + chain_offset_y];
                }
            }
        }

        // Do actions given the context --

        // Editing
        if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_a_down())
        {
            // Edit actual chain
            if (cursor_x == 0)
            {
                // Movement keys
                if (goup || godown || goright || goleft)
                {
                    // Set to 0 if it isn't set
                    if (chainlist[open_chain]->arr[cursor_y + chain_offset_y] == -1)
                        chainlist[open_chain]->arr[cursor_y + chain_offset_y] = 0x0000;

                    // Mod the left two digits
                    if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_shift_down())
                    {
                        if (goup) chainlist[open_chain]->arr[cursor_y + chain_offset_y] += 0x1000;
                        if (godown) chainlist[open_chain]->arr[cursor_y + chain_offset_y] -= 0x1000;
                        if (goright) chainlist[open_chain]->arr[cursor_y + chain_offset_y] += 0x0100;
                        if (goleft) chainlist[open_chain]->arr[cursor_y + chain_offset_y] -= 0x0100;
                    }
                    // Mod the right two digits
                    else
                    {
                        if (goup) chainlist[open_chain]->arr[cursor_y + chain_offset_y] += 0x0010;
                        if (godown) chainlist[open_chain]->arr[cursor_y + chain_offset_y] -= 0x0010;
                        if (goright) chainlist[open_chain]->arr[cursor_y + chain_offset_y] += 0x0001;
                        if (goleft) chainlist[open_chain]->arr[cursor_y + chain_offset_y] -= 0x0001;
                    }

                    // Wrap the cell between 0x0000 and 0xFFFF
                    if (chainlist[open_chain]->arr[cursor_y + chain_offset_y] < 0)
                        chainlist[open_chain]->arr[cursor_y + chain_offset_y] = 0xFFFF + (chainlist[open_chain]->arr[cursor_y + chain_offset_y] + 1);
                    if (chainlist[open_chain]->arr[cursor_y + chain_offset_y] > 0xFFFF)
                        chainlist[open_chain]->arr[cursor_y + chain_offset_y] = (chainlist[open_chain]->arr[cursor_y + chain_offset_y] - 1) - 0xFFFF;

                    // Copy to clipboard
                    copied_phrase = chainlist[open_chain]->arr[cursor_y + chain_offset_y];
                }

                // Handle deletes
                if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_b_pressed())
                    chainlist[open_chain]->arr[cursor_y + chain_offset_y] = -1;
            }
            // Edit transpose
            else
            {
                // Movement keys
                if (goup || godown || goright || goleft)
                {
                    // Mod the left two digits
                    if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_shift_down())
                    {
                        if (goup) chainlist[open_chain]->arr_transpose[cursor_y + chain_offset_y] += 0x1000;
                        if (godown) chainlist[open_chain]->arr_transpose[cursor_y + chain_offset_y] -= 0x1000;
                        if (goright) chainlist[open_chain]->arr_transpose[cursor_y + chain_offset_y] += 0x0100;
                        if (goleft) chainlist[open_chain]->arr_transpose[cursor_y + chain_offset_y] -= 0x0100;
                    }
                    // Mod the right two digits
                    else
                    {
                        if (goup) chainlist[open_chain]->arr_transpose[cursor_y + chain_offset_y] += 0x0010;
                        if (godown) chainlist[open_chain]->arr_transpose[cursor_y + chain_offset_y] -= 0x0010;
                        if (goright) chainlist[open_chain]->arr_transpose[cursor_y + chain_offset_y] += 0x0001;
                        if (goleft) chainlist[open_chain]->arr_transpose[cursor_y + chain_offset_y] -= 0x0001;
                    }

                    // Wrap the cell between 0x0000 and 0xFFFF
                    if (chainlist[open_chain]->arr_transpose[cursor_y + chain_offset_y] < 0)
                        chainlist[open_chain]->arr_transpose[cursor_y + chain_offset_y] = 0xFFFF + (chainlist[open_chain]->arr_transpose[cursor_y + chain_offset_y] + 1);
                    if (chainlist[open_chain]->arr_transpose[cursor_y + chain_offset_y] > 0xFFFF)
                        chainlist[open_chain]->arr_transpose[cursor_y + chain_offset_y] = (chainlist[open_chain]->arr_transpose[cursor_y + chain_offset_y] - 1) - 0xFFFF;
                }
            }
        }
        // Goto Song Editor
        else if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_select_down())
        {
            // Go to the left page over
            if (goleft)
            {
                // Song
                state = m_song;
                breakend = true;
            }
        }
        // Moving
        else
        {
            cursor_x += goright - goleft;
            cursor_y += godown - goup;
        }

        // wrap the cursor and clamp offsets
        if (cursor_x > chain_grid_w - 1)
            cursor_x = 0;
        if (cursor_x < 0)
            cursor_x = chain_grid_w - 1;
        if (cursor_y + chain_offset_y > chain::length - 1)
        {
            cursor_y = 0;
            chain_offset_y = 0;
        }
        if (cursor_y + chain_offset_y < 0)
        {
            cursor_y = chain_grid_h - 1;
            chain_offset_y = chain::length - chain_grid_h;
        }

        // Move the page
        if (cursor_y > chain_grid_h - 1)
            chain_offset_y += 1;
        if (cursor_y < 0)
            chain_offset_y -= 1;

        cursor_y = SDL_clamp(cursor_y, 0, chain_grid_h - 1);
        chain_offset_y = SDL_clamp(chain_offset_y, 0, chain::length - chain_grid_h);

        // Do we want to update the UI?
        DrawChainUI(chain_offset_y, "all");
    }

    // Handle repeating movement from hold presses
    HandleMovementRepeaters();
}

// Master pre code
void GameInit()
{
    // Set song grid w and h
    song_grid_h = geptr->GetCanvasH() - 4;
    song_grid_w = (geptr->GetCanvasW() - 5) / 5;
    chain_grid_h = 16;
    chain_grid_w = 2;

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
    if (state == m_song)
        DrawSongUI(0, 0, "all");
    if (state == m_chain)
        DrawChainUI(0, "all");

    // Test: Init synth and play it
    synptr2 = new GravityEngine_Synth();
    synptr2->pulse_width_freq = 0.5f;
    synptr2->panning = 0.5f;
    synptr2->freq = 261.63;
    synptr2->volume = 0;
    synptr2->volume_freq = -50;
    synptr2->waveform = triangle;
    // geptr->BindSynthToChannel(synptr2, 0);

    // Test: Init file play and play it
    int i = geptr->AddSound("DrumBeat.wav");
    // geptr->PlaySoundOnChannel(0, 1, true);

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
    // Get input and apply to the editor as appropriate
    EditorControl();
}

// Master post code
void PostGameLoop()
{
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
    // Free all lists
    for (auto o : chainlist)
        delete o;
    for (auto o : phraselist)
        delete o;
    for (auto o : instrumentlist)
        delete o;
    for (auto o : tablelist)
        delete o;
    running = false;

    // Report success to host
    return 0;
}