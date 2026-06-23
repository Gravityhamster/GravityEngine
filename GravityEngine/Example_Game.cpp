#include "GravityEngineSDL.h"

GravityEngine_Core* geptr;
GravityEngine_Synth* synptr2;
bool s2isplaying = 0;
double global_timer = 1;
bool was = 0;
bool is = 0;
int bpm = 60; // Beats per minute
int fps = 360; // Frame rate in hz
int tps = 6; // Ticks per step
double frametick = 0;
int songgrid[0xffff][64];

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

// TODO: Gravityengine objects if necessary
class player : public virtual GravityEngine_Object
{
    private:
	public:
        player() {};
		~player() {};
		void begin_step() {};
		void step() {};
		void end_step() {};
};

// Get note freq
double NoteFreq(int n)
{
    // https://superglobalcalculator.com/calculators/music/piano-key-frequency/
    return 440.0 * pow(2.0, (n - 49.0) / 12.0);
}

// Beats-per-minute to Tick frequency
// b : bpm
// f : fps
double BpmToFrametick(int b, int f)
{
    // 60 beats / 1 minute
    // 60 beats / 60 seconds
    // 1 beat / 1 second
    // 6 ticks / 60 frames
    // 0.1 tick / 1 frame
    // 1 tick every 10 frames

    double x = b;
    x /= 60.0; // bpm / 60 seconds = bps
    x *= tps; // 6 ticks per beat
    x /= f; // ticks per frame

    return x;
}

// Execute tick
void DoTick()
{
    // TODO: Handle playing the music
    // geptr->DrawTextString(5, 6, geptr->entity, std::to_string(synptr2->freq), { {255,255,255},{0,0,0} });
    synptr2->volume = 0.25;
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
    // Menu title
    if (str_contains(type, "all") || str_contains(type, "title"))
    {
        geptr->DrawTextString(0, 1, geptr->background, "SONG", primary_text_a);
    }

    // Row numbers
    if (str_contains(type, "all") || str_contains(type, "y"))
    {
        // Height of the screen
        int h = geptr->GetCanvasH() - 4;

        // Draw the row numbers
        for (int i = 0; i < h; i++)
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
        // Width of the screen
        int w = (geptr->GetCanvasW() - 5) / 5;

        // Draw the headers
        for (int i = 0; i < w; i++)
        {
            std::string tempstr = std::to_string(i + off_x);
            tempstr.insert(tempstr.begin(), 2 - tempstr.size(), '0');
            geptr->DrawTextString(5 + i*5, 2, geptr->background, "CH" + tempstr, header_text_a);
        }
    }

    // Song grid navigator
    if (str_contains(type, "all") || str_contains(type, "navigator"))
    {
        // Width and height of the screen
        int h = geptr->GetCanvasH() - 4;
        int w = (geptr->GetCanvasW() - 5) / 5;

        //Draw the song grid
        for (int y = 0; y < h; y++)
        {
            for (int x = 0; x < w; x++)
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

// Master pre code
void GameInit()
{
    // Init song phrase list
    for (int y = 0; y < 0xffff; y++)
    {
        for (int x = 0; x < 64; x++)
        {
            songgrid[y][x] = -1;
        }
    }

    // Start song UI
    DrawSongUI(0, 0, "all");

    // Test: Init synth and play it
    synptr2 = new GravityEngine_Synth();
    synptr2->pulse_width_freq = 0.5f;
    synptr2->freq = 261.63;
    synptr2->volume = 0.25;
    synptr2->volume_freq = -10;
    synptr2->waveform = triangle;
    geptr->BindSynthToChannel(synptr2, 0);
}

// Master pre code
void PreGameLoop()
{
    // Show frame step calculation
    // geptr->DrawTextString(5, 5, geptr->entity, "TICKS PER FRAME: " + std::to_string(BpmToFrametick(bpm, fps)), { {255,0,255},{0,0,0} });
    frametick = BpmToFrametick(bpm, fps);

    // Handle music clock
    global_timer += frametick;
    if (global_timer >= 1)
    {
        DoTick();
        while (global_timer >= 1)
            global_timer -= 1;
    }
}

// Master post code
void PostGameLoop()
{
}

int main()
{
    // Init engine - 128x72 is generally the largest you can get and still maintain good performance
    GravityEngine_Core ge_inst = GravityEngine_Core("Game", "com.example.game", "1.0", 96/2, 54/2, fps, 1920, 1080, "./GameFont.ttf", 64);

    ge_inst.debug_mode = true; // Show debug overlay
    ge_inst.debug_complex = false; // Show all information
    geptr = &ge_inst; // Set the pointer to the console engine class

    // Start game loop
    ge_inst.Start(&GameInit, &PreGameLoop, &PostGameLoop);

    // Report success to host
    return 0;
}