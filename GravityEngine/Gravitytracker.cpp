#include "GravityEngineSDL.h"
#include <list>

GravityEngine_Core* geptr;

// Editor held modifier
enum edit_mod
{
    left,
    right,
    center
};

// Playing type
enum playing_type
{
    pt_song,
    pt_chain,
    pt_phrase,
    pt_chain_all,
    pt_phrase_all
};

// Global variables --
int ticknumber = 0; // Track tick progress
int cursor_x; // X location of the user's cursor
int cursor_y; // Y location of the user's cursor
int offset_x; // X offset of the editor scroll
int offset_y; // Y offset of the editor scroll
int chain_offset_y; // Y offset when editing the chain
int phrase_offset_y; // Y offset of the editor scroll
int sample_offset_y; // Y offset of the samples selector scroll
int inputholdtimer = 0; // The timer for checking if an input should be considered held-down
int inputholdthreshold = 15; // Frames til in input should start repeating
int inputholddelay = 2; // How many frames to skip on hold (2 == every other, 3 == every other 3, etc.) 
const int channelcount = 64; // How many audio channels in the song
int rowcount = 0xffff; // How many rows in the song - 65535 chains * 16 phrases * 16 steps = 16776960 steps / 4 steps = 4194240 beats
int bpm = 155; // 170; // Beats per minute of the song
int tps = 6; // Ticks per step of the song
int fps = 60; // Frame rate in hz of the UI
double ticklength = 0; // Nanoseconds per tick
int song_grid_h; // Height of the Song Editor UI
int song_grid_w; // Width of the Song Editor UI
int chain_grid_h; // Height of the Chain Editor UI
int chain_grid_w; // Width of the Chain Editor UI
int phrase_grid_h; // Height of the Phrase Editor UI
int phrase_grid_w; // Width of the Phrase Editor UI
bool running = false; // Is the song currently playing?
std::thread* timing_thread; // Thread to play ticks
edit_mod leftrightcenter = center; // Editing state for which part of the number we are editing
int copied_chain = -1; // Clipboard for copying a chain
int copied_phrase = -1; // Clipboard for copying a phrase
int copied_note = -9999; // Clipboard for copying a note
int copied_instr = -1; // Clipboard for copying an instrument
int copied_effect = -1; // Clipboard for copying an instrument
int copied_effect_param = -1; // Clipboard for copying an instrument
int copied_smple = -1; // Clipboard for copying a sample
int open_channel = -1; // Track which channel we've opened on
int open_chain = -1; // Tracking which chain we have open
int open_chain_index = 0; // Index of chain in the song
int open_phrase = -1; // Tracking which phrase we have open
int open_phrase_index = 0; // Index of phrase in the chain in the song
int playing_channel = -1; // Track which channel we're playing
int open_instrument = -1; // Tracking which instrument we have open
int open_sample = -1; // Tracking which sample we have open
char fx[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G'}; // List of effects
int min_note = -12; // Minimum note that can be inserted
int max_note = 107; // Maximum note that can be inserted
bool pause_song = true; // Pause the song progression
bool play_thread = false; // Play thread check flag
int do_deep_copy = 0; // Track progress for deep copy
float instrument_edit_digit_count = -1; // Flag for how many digits to expect an edit to be in the instr editor
int instrument_edit_y = -1;
int file_display_count = 10;
int current_dir_length = 0;
std::string main_dir = ".\\SampleLibrary\\";
std::string current_dir = main_dir;
std::string selected_path = "";
bool selected_path_isdir = false;
bool alt_mode = false;
playing_type play_context = pt_song; // What type of play are we doing

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
            // Fill the phrase with blanks
            for (int y = 0; y < len_y; y++)
                for (int x = 0; x < len_x; x++)
                    arr[y][x] = x == 0 ? -9999 : -1;
        };

        // Destruct phrase
        ~phrase() {};
}; 

// Samples - Sound sample references
class sample
{
    public:
        std::string path;
        int sound_index = -1;

        // Methods
        sample* DeepCopySample()
        {
            // New sample
            auto i = new sample();

            // Apply parameters
            i->path = path;
            i->sound_index = sound_index;

            // Return copy
            return i;
        }
};


// Get note freq
double NoteFreq(double n)
{
    // https://superglobalcalculator.com/calculators/music/piano-key-frequency/
    return 440.0 * pow(2.0, ((n+1) - 49.0) / 12.0);
}

// Get freq note
double FreqNote(double f)
{
    return 12 * std::log2(f / 440) + 48;
}

// Get sample ratio
// int base_pitch : Base pitch of the sample tuned to a piano. For example, C4 == 39
// int new_pitch : New pitch of the sample tuned to a piano. For example, C#4 == 40
// int detune : Cents to detune the pitch by
double GetSampleRatioChange(int base_pitch, int new_pitch, double detune = 0)
{
    return std::pow(2, (FreqNote(NoteFreq(new_pitch) + detune) - base_pitch) / 12.f);
}

// Instruments - Note audio definitions
class instrument 
{
    public:
        // Audio parameters
        static const int synth_menu_width = 1;
        static const int synth_menu_height = 10;
        static const int sample_menu_width = 1;
        static const int sample_menu_height = 13;

        // Note to self: 0x80 (128) is the middle number in 0xFF (255).

        // Synth or Sample
        ChannelType type = ChannelType::synth;

        // Parameters for both
        int detune_edit = 0x80; // 0x80 = 0, 0x00 = -128, 0xFF = 127
        float detune = 0.f; // Add to freq
        int volume_edit = 0xFF80; // 0xFF = 1 | 0x80 = 0
        float volume = 1.f; // Volume
        float volume_freq = 0.0; // Linear volume sweep (positive up, negative down)
        int pan_edit = 0x8000; // 0x80 = 0.5, 0x00 = 0;
        float panning = 0.5f; // Panning amount (0.0 = L, 0.5 = C, 1.0 = R)
        float pan_freq = 0.0; // Ping-pong pan frequency
        int pitch_freq_edit = 0x8000; // 0x8000 = 0, 0x0000 = -32768, 0xFFFF = 32767
        float pitch_freq = 0.0; // Linear pitch sweep speed (positive up, negative down)
        int cutoff_edit = 0x8000; // 0x0000 = 0, 0xFFFF = 1
        float cutoff = 0.0f; // Filter cutoff
        int resonance_edit = 0x6554; // 0x0000 = 0, 0xFFFF = 1
        float resonance = 0.0f; // Filter resonance
        SynthWaveForm waveform = SynthWaveForm::sine; // Synth wave type
        FilterType filter = FilterType::none; // Lowpass, bandpass, or highpass filter

        // Synth parameters
        int pw_edit = 0x8000; // 0x80 = 0.5, 0x00 = 0;
        float pulse_width = 0.5; // Pulse Width (Only for Pulse Wave Synth)
        float pulse_width_freq = 0.0; // Ping-pong pulse-width pan frequency (Only for Pulse Wave Synth)

        // Sample parameters
        int base_pitch = 39;
        int sample_index = -1;
        int start_time_ms = 0x000000;
        int mid_time_ms = 0x000000;
        int end_time_ms = 0xFFFFFF;
        int loop = false;

        // Methods
        instrument* DeepCopyInstrument()
        {
            // New instrument
            auto i = new instrument();

            // Apply parameters
            i->type = type;
            i->detune_edit = detune_edit;
            i->detune = detune;
            i->volume_edit = volume_edit;
            i->volume = volume;
            i->volume_freq = volume_freq;
            i->pan_edit = pan_edit;
            i->panning = panning;
            i->pan_freq = pan_freq;
            i->pw_edit = pw_edit;
            i->pulse_width = pulse_width;
            i->pulse_width_freq = pulse_width_freq;
            i->pitch_freq_edit = pitch_freq_edit;
            i->pitch_freq = pitch_freq;
            i->cutoff_edit = cutoff_edit;
            i->cutoff = cutoff;
            i->resonance_edit = resonance_edit;
            i->resonance = resonance;
            i->waveform = waveform;
            i->filter = filter;
            i->base_pitch = base_pitch;
            i->sample_index = sample_index;
            i->start_time_ms = start_time_ms;
            i->end_time_ms = end_time_ms;
            i->loop = loop;

            // Return copy
            return i;
        }
};

// Tables - List of modulations for the currently playing instrument
class table 
{ 
    public:
        static const int len_x = 7;
        static const int len_y = 16;
        int arr[len_y][len_x]; // 7w x 16h - List of ticks for sound automation 

        // Create phrase
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

// SampleAutomator - Automation related to samples on channels
class sampleautomator
{
public:
    int channelnumber = -1; // Which channel is this automator assigned to?
    float volume = 1.0f;
    float volume_freq = 0;
    int base_pitch = 0;
    float freq = 1.0f;
    float pitch_freq = 0.0f;

    // Run the channel automation
    void ChannelAutomation()
    {
        // Step volumne
        if (volume_freq != 0)
            volume += volume_freq / 100;
        if (volume < 0)
            volume = 0;
        // Step pitch
        if (pitch_freq != 0)
        {
            if (pitch_freq > 0)
                freq = freq * (pitch_freq + 1);
            if (pitch_freq < 0)
                freq = freq / (abs(pitch_freq) + 1);
        }

        // Apply volume changes
        geptr->SetChannelVolume(channelnumber, volume * 4);
        // Apply pitch changes
        geptr->SetChannelPitchRatio(channelnumber, freq);
    }
};

// Sample automator variable
sampleautomator sampleautomatorlist[channelcount];

// Data structures --
int** songgrid; //[0xffff][64];
std::vector<chain*> chainlist;
std::vector<phrase*> phraselist;
std::vector<instrument*> instrumentlist;
std::vector<sample*> samplelist;
std::vector<table*> tablelist;
GravityEngine_Synth* synthlist[channelcount];

// Tracker colors --
color primary_text_a = { {255, 255, 255}, {0, 0, 0} };
color header_text_a = { {255, 255, 255}, {0, 0, 100} };
color primary_text_b = { {0, 0, 0}, {255, 255, 255} };
color header_text_b = { {0, 0, 100}, {255, 255, 255} };
color body_text_a = { {255, 255, 255}, {0, 0, 0} };
color body_text_b = { {0, 0, 0}, {255, 255, 255} };


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
    if (vec->size() <= index)
        vec->insert(vec->begin() + index, ptr);
    else
        (*vec)[index] = ptr;
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

// Channel list type pointer
ChannelType* channellisttypeptr[channelcount];

// Play step phrase
// channelnumber : The particular channel to play the step on
// playing_phrase : Phrase to play
// step_ptr : Phrase progress index
void PlayStepPhrase(int channel_index, int playing_phrase, int step_ptr, double pitch_offset = 1)
{
    // Get frequency to play
    auto f = phraselist[playing_phrase]->arr[step_ptr][0];
    auto i = phraselist[playing_phrase]->arr[step_ptr][1];
    // If no note is present, no need to play
    if (f != -9999 && GetAt(&instrumentlist, i) != nullptr)
    {
        // TODO: Implement instrument parameters
        // TODO: Sub-step on preview so that we can preview the table commands as well
        (*channellisttypeptr[channel_index]) = instrumentlist[i]->type;
        if (instrumentlist[i]->type == ChannelType::synth)
        {
            geptr->SetChannelVolume(channel_index, 1);
            geptr->SetChannelPitchRatio(channel_index, 1);
            synthlist[channel_index]->freq = NoteFreq(f) + instrumentlist[i]->detune;
            synthlist[channel_index]->volume = instrumentlist[i]->volume;
            synthlist[channel_index]->volume_freq = instrumentlist[i]->volume_freq;
            synthlist[channel_index]->panning = instrumentlist[i]->panning;
            synthlist[channel_index]->pan_freq = instrumentlist[i]->pan_freq;
            synthlist[channel_index]->pulse_width = instrumentlist[i]->pulse_width;
            synthlist[channel_index]->pulse_width_freq = instrumentlist[i]->pulse_width_freq;
            synthlist[channel_index]->pitch_freq = instrumentlist[i]->pitch_freq;
            synthlist[channel_index]->cutoff = instrumentlist[i]->cutoff;
            synthlist[channel_index]->resonance = instrumentlist[i]->resonance;
            synthlist[channel_index]->waveform = instrumentlist[i]->waveform;
            synthlist[channel_index]->filter = instrumentlist[i]->filter;
            geptr->BindSynthToChannel(synthlist[channel_index], channel_index);
        }
        else if (instrumentlist[i]->type == ChannelType::file)
        {
            geptr->SetChannelPitchRatio(channel_index, GetSampleRatioChange(instrumentlist[i]->base_pitch, f, instrumentlist[i]->detune));
            geptr->SetChannelVolume(channel_index, instrumentlist[i]->volume*4);
            geptr->SetChannelTimeOffsets(channel_index, instrumentlist[i]->start_time_ms, instrumentlist[i]->mid_time_ms, instrumentlist[i]->end_time_ms);
            geptr->SetChannelPanning(channel_index, instrumentlist[i]->panning);
            geptr->PlaySoundOnChannel(samplelist[instrumentlist[i]->sample_index]->sound_index, channel_index, instrumentlist[i]->loop);
            sampleautomatorlist[channel_index].volume = instrumentlist[i]->volume;
            sampleautomatorlist[channel_index].volume_freq = instrumentlist[i]->volume_freq;
            sampleautomatorlist[channel_index].freq = GetSampleRatioChange(instrumentlist[i]->base_pitch, f, instrumentlist[i]->detune);
            sampleautomatorlist[channel_index].pitch_freq = instrumentlist[i]->pitch_freq;
        }
    }
}

// Play step chain
// channelnumber : The particular channel to play the step on
// playing_chain : Chain to play
// phrase_ptr : Chain progress index
// step_ptr : Phrase progress index
void PlayStepChain(int channel_index, int playing_chain, int phrase_ptr, int step_ptr)
{
    // Play the step at this chain position
    auto play_phrase = chainlist[playing_chain]->arr[phrase_ptr];
    if (play_phrase != -1 && GetAt(&phraselist, play_phrase) != nullptr)
        PlayStepPhrase(channel_index, play_phrase, step_ptr);
}

// Play step song
// channelnumber : The particular channel to play the step on
// chain_ptr : Song progress index
// phrase_ptr : Chain progress index
// step_ptr : Phrase progress index
void PlayStepSong(int channel_index, int chain_ptr, int phrase_ptr, int step_ptr)
{
    // Play the step at this song position
    auto play_chain = songgrid[chain_ptr][channel_index];
    if (play_chain != -1 && GetAt(&chainlist, play_chain) != nullptr)
        PlayStepChain(channel_index, play_chain, phrase_ptr, step_ptr);
}

// ChannelSequencer - Track position of the channel in time in the song
class channelsequencer
{
public:
    ChannelType type = ChannelType::synth;
    std::atomic<int> channelnumber = -1; // Which channel is this sequencer assigned to?
    std::atomic<int> chain_ptr = 0; // Int position of the channel in the song
    std::atomic<int> phrase_ptr = 0; // Int position of the channel in the chain
    std::atomic<int> step_ptr = 0; // Int position of the channel in the phrase
    std::atomic<int> tick_ptr = 0; // Int position of the channel in the table
    std::atomic<int> playing_chain = -1;
    std::atomic<int> playing_phrase = -1;
    std::atomic<bool> cant_play = false;

    // Count tick
    void sub_step()
    {
        // If channel is in a locked state, do not play in the song context
        if (cant_play == true && play_context == pt_song)
            return;

        // TODO : Substep-level effects

        // Increment tick in table
        tick_ptr++;
        // Go back to the 0th tick if we have reached the end of the table
        tick_ptr = tick_ptr % table::len_y;
    }

    // Count step
    void step()
    {
        if (cant_play == true && play_context == pt_song)
            return;

        // TODO : Play step and sub-step-level effects

        // Play context within this local phrase
        if ((play_context == pt_phrase && playing_channel == channelnumber) || play_context == pt_phrase_all)
        {
            PlayStepPhrase(channelnumber, playing_phrase, step_ptr);
        }
        if ((play_context == pt_chain && playing_channel == channelnumber) || play_context == pt_chain_all)
        {
            PlayStepChain(channelnumber, playing_chain, phrase_ptr, step_ptr);
        }
        if (play_context == pt_song && !cant_play)
        {
            PlayStepSong(channelnumber, chain_ptr, phrase_ptr, step_ptr);
        }

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

    // Prepare for playing the song
    // int start_point : Where to start in the channel's chain list
    bool init_song_play(int start_point)
    {
        chain_ptr = start_point;
        phrase_ptr = 0;
        step_ptr = 0;
        tick_ptr = 0;

        // Keep moving back til we get to a valid play point to play at
        do
        {
            if (chain_ptr > 0 && cant_play)
                chain_ptr--;
            // Report for now that we can play
            cant_play = false;
            // Is there a channel reference here?
            if (songgrid[chain_ptr][channelnumber] == -1)
                cant_play = true;
            else
            {
                // Does the chain exist
                auto c = GetAt(&chainlist, songgrid[chain_ptr][channelnumber]);
                if (c == nullptr)
                    cant_play = true;
                else
                {
                    // Is there a phrase here?
                    auto p = GetAt(&phraselist, c->arr[phrase_ptr]);
                    if (p == nullptr)
                        cant_play = true;
                }
            }
        } while (chain_ptr > 0 && cant_play);

        // Report that this channel needs to sit out
        return cant_play;
    }

private:

    void inc_phrase()
    {
        // If we are playing only the phrase, we don't want to increment
        if (play_context != pt_phrase && play_context != pt_phrase_all)
        {
            // Increment phrase in chain
            phrase_ptr++;
            if (phrase_ptr % chain::length == 0)
            {
                phrase_ptr = 0;
                // Increment the chain pointer in the song
                inc_chain();
            }
            else
            {
                // Check if the channel is checking a valid chain
                auto play_chain = GetAt(&chainlist, songgrid[chain_ptr][channelnumber]);
                if (play_chain != nullptr)
                {
                    // If the channel is currently on a null phrase, go back to start
                    if (play_chain->arr[phrase_ptr] == -1)
                    {
                        phrase_ptr = 0;
                        // Increment the chain pointer in the song
                        inc_chain();
                    }
                }
            }
        }
    }

    void inc_chain()
    {
        // If we are playing only the chain, we don't want to increment
        // We don't need to check phrase because this function is only called from phrase
        if (play_context != pt_chain && play_context != pt_chain_all)
        {
            // Increment chain in song
            chain_ptr++;
            // If the chain ptr has reached the end of the song, go back to the 0th chain
            if (chain_ptr % rowcount == 0)
            {
                chain_ptr = 0;
            }

            // If we are on an empty chain or phrase, move up til we find the beginning of the list of consecutive chains
            bool need_to_go_back = false;
            // Check if the channel is checking a valid chain
            {
                auto play_chain = GetAt(&chainlist, songgrid[chain_ptr][channelnumber]);
                if (play_chain != nullptr)
                {
                    auto play_phrase = GetAt(&phraselist, play_chain->arr[phrase_ptr]);
                    if (play_phrase == nullptr)
                        need_to_go_back = true;
                }
                else
                {
                    need_to_go_back = true;
                }
            }

            // If we need to go back...
            if (need_to_go_back)
            {
                // Go back a step
                chain_ptr--;
                // Keep going back every step until we have made it to the beginning or we have found the last blank
                while (need_to_go_back && chain_ptr != -1)
                {
                    // Check if the chain is valid
                    auto play_chain = GetAt(&chainlist, songgrid[chain_ptr][channelnumber]);
                    if (play_chain != nullptr)
                    {
                        // Check if the phrase is valid
                        auto play_phrase = GetAt(&phraselist, play_chain->arr[phrase_ptr]);
                        if (play_phrase == nullptr)
                            need_to_go_back = false; // Valid
                    }
                    else
                    {
                        need_to_go_back = false; // Valid
                    }
                    // If not valid, check next chain back
                    if (need_to_go_back)
                        chain_ptr--;
                }
                // Get back to the start
                chain_ptr++;
            }
        }
    }
};

// Channel sequencer variable
channelsequencer channellist[channelcount];

// Deep Copy Phrase
// phrase_index : The ID of the phrase
int DeepCopyPhrase(int phrase_index)
{
    // Try get the phrase 
    auto phrase_to_copy = GetAt(&phraselist, phrase_index);

    // If the phrase exists, recreate it and return the new index
    if (phrase_to_copy != nullptr)
    {
        // Create new
        phrase* target_phrase = new phrase();
        // Copy the phrase
        for (int i = 0; i < phrase::len_x; i++)
            for (int q = 0; q < phrase::len_y; q++)
                target_phrase->arr[q][i] = phrase_to_copy->arr[q][i];
        // Insert the phrase into the phraselist
        auto n = GetNextEmpty(&phraselist);
        InsertAt(&phraselist, n, target_phrase);
        // Return the location of the new phrase
        return n;
    }
    else
    {
        return -1;
    }
}

// Shallow Copy Chain
// chain_index : The ID of the chain
int ShallowCopyChain(int chain_index)
{
    // Try get the chain 
    auto chain_to_copy = GetAt(&chainlist, chain_index);

    // If the chain exists, recreate it and return the new index
    if (chain_to_copy != nullptr)
    {
        // Create new
        chain* target_chain = new chain();
        // Copy the chain
        for (int i = 0; i < chain::length; i++)
        {
            // Copy values
            target_chain->arr[i] = chain_to_copy->arr[i];
            target_chain->arr_transpose[i] = chain_to_copy->arr_transpose[i];
        }
        // Insert the chain into the chainlist
        auto n = GetNextEmpty(&chainlist);
        InsertAt(&chainlist, n, target_chain);
        // Return the location of the new chain
        return n;
    }
    else
    {
        return -1;
    }
}

// Deep Copy Chain
// chain_index : The ID of the chain
int DeepCopyChain(int chain_index)
{
    // First Shallow Copy the Chain
    int r = ShallowCopyChain(chain_index);

    // Check return value
    if (r != -1)
    {
        // Clone the phrases within the chain
        for (int i = 0; i < chain::length; i++)
        {
            // Only clone if the index exists
            if (GetAt(&phraselist, chainlist[r]->arr[i]) != nullptr)
                chainlist[r]->arr[i] = DeepCopyPhrase(chainlist[r]->arr[i]);
            // Insert new if not empty but not exist
            else if (chainlist[r]->arr[i] != -1)
                chainlist[r]->arr[i] = GetNextEmpty(&phraselist);
        }
    }

    // Return the chain index
    return r;
}

// Song editor menu
enum menu
{
    m_song,
    m_chain,
    m_phrase,
    m_instrument,
    m_options,
    m_table,
    m_wave
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
        bool is_up_released() { return was_up && !is_up; }

        bool is_down_pressed() { return is_down && !was_down; }
        bool is_down_down() { return is_down; }
        bool is_down_released() { return was_down && !is_down; }

        bool is_left_pressed() { return is_left && !was_left; }
        bool is_left_down() { return is_left; }
        bool is_left_released() { return was_left && !is_left; }

        bool is_right_pressed() { return is_right && !was_right; }
        bool is_right_down() { return is_right; }
        bool is_right_released() { return was_right && !is_right; }

        bool is_a_pressed() { return is_a && !was_a; }
        bool is_a_down() { return is_a; }
        bool is_a_released() { return was_a && !is_a; }

        bool is_b_pressed() { return is_b && !was_b; }
        bool is_b_down() { return is_b; }
        bool is_b_released() { return was_b && !is_b; }

        bool is_start_pressed() { return is_start && !was_start; }
        bool is_start_down() { return is_start; }
        bool is_start_released() { return was_start && !is_start; }

        bool is_select_pressed() { return is_select && !was_select; }
        bool is_select_down() { return is_select; }
        bool is_select_released() { return was_select && !is_select; }

        bool is_shift_pressed() { return is_shift && !was_shift; }
        bool is_shift_down() { return is_shift; }
        bool is_shift_released() { return was_shift && !is_shift; }
};

// Input getter
int inputgetter;

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
        // Step the channel sequencers
        for (int i = 0; i < channelcount; i++)
            if (!pause_song) channellist[i].step();
    }
    
    // Do Sub-step Code --

    // Tick the channel sequencers
    for (int i = 0; i < channelcount; i++)
    {
        if (!pause_song) channellist[i].sub_step();
        if (channellist[i].type == ChannelType::synth) synthlist[i]->SynthAutomation(); // Run synth automation on animated variables
        if (channellist[i].type == ChannelType::file && state != m_wave) sampleautomatorlist[i].ChannelAutomation(); // Run sample automation on animated variables
    }

    // Increment global song position in ticks --
    if (!pause_song) ticknumber++;
}

// Convert i to hex string
// int i : Number to convert
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
void DrawSongUI(int off_x, int off_y)
{
    // Width and height of the screen
    int h = song_grid_h;
    int w = song_grid_w;

    // Song position pointer
    if (play_context == pt_song &&
        pause_song == false)
    {
        // Loop through all visible channels
        for (int x = 0; x < w && x + off_x < channelcount; x++)
        {
            // Only draw if the channel has reported that it is allowed to play
            if (channellist[x + off_x].cant_play == false)
            {
                geptr->DrawChar(4 + x * 5, 3 + channellist[x + off_x].chain_ptr, geptr->entity, '>');
                geptr->DrawSetColor(4 + x * 5, 3 + channellist[x + off_x].chain_ptr, geptr->entity, primary_text_a);
            }
        }
    }

    // Menu title
    geptr->DrawTextString(0, 1, geptr->entity, "SONG", primary_text_a);

    // Draw the row numbers
    for (int i = 0; i < h && i + off_y < rowcount; i++)
    {
        int tempint = i + off_y;

        auto upperstr = IntToHexString(tempint);

        upperstr.insert(upperstr.begin(), 4 - upperstr.size(), '0');
        geptr->DrawTextString(0, 3 + i, geptr->entity, upperstr, primary_text_a);
    }

    // Channel headers
    for (int i = 0; i < w && i + off_x < channelcount; i++)
    {
        std::string tempstr = std::to_string(i + off_x);
        tempstr.insert(tempstr.begin(), 2 - tempstr.size(), '0');
        geptr->DrawTextString(5 + i*5, 2, geptr->entity, "CH" + tempstr, header_text_a);
    }

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

// Draw Chain Editor UI
// off_y : UI offset on the y axis
void DrawChainUI(int off_y)
{
    // Width and height of the screen
    int h = chain_grid_h;
    int w = 1;

    // Song position pointer
    if (((open_channel == playing_channel && open_chain_index == channellist[playing_channel].chain_ptr)
        || (channellist[open_channel].chain_ptr == open_chain_index && channellist[open_channel].cant_play == false && play_context == pt_song)) && 
        play_context != pt_phrase && play_context != pt_phrase_all &&
        pause_song == false)
    {
        geptr->DrawChar(4, 3 + channellist[playing_channel].phrase_ptr, geptr->entity, '>');
        geptr->DrawSetColor(4, 3 + channellist[playing_channel].phrase_ptr, geptr->entity, primary_text_a);
    }

    // Draw menu title and chain number
    auto outstr = IntToHexString(open_chain);
    outstr.insert(outstr.begin(), 4 - outstr.size(), '0');
    geptr->DrawTextString(0, 1, geptr->entity, "CHAIN - " + outstr, primary_text_a);

    // Draw the row numbers
    for (int i = 0; i < h && i + off_y < rowcount; i++)
    {
        int tempint = i + off_y;

        auto upperstr = IntToHexString(tempint);

        upperstr.insert(upperstr.begin(), 4 - upperstr.size(), '0');
        geptr->DrawTextString(0, 3 + i, geptr->entity, upperstr, primary_text_a);
    }

    // Channel headers
    geptr->DrawTextString(5, 2, geptr->entity, "PHSE", header_text_a);
    geptr->DrawTextString(10, 2, geptr->entity, "TRPS", header_text_a);

    //Draw the chain grid
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

// Converts an integer to a note string
// int note : Piano key number
std::string IntToNoteString(int n)
{
    // Get the octave
    int octave = ((n-3) >= 0 ? std::floor((n-3) / 12) + 1 : 0);
    char octave_char = 48 + octave;
    // Get the note letter
    int note_int = (n >= 0 ? n : n + 12) % 12;
    std::string note_char = "";
    // Get the note string
    switch(note_int)
    {
        case 0:
            note_char = "A-";
            break;
        case 1:
            note_char = "A#";
            break;
        case 2:
            note_char = "B-";
            break;
        case 3:
            note_char = "C-";
            break;
        case 4:
            note_char = "C#";
            break;
        case 5:
            note_char = "D-";
            break;
        case 6:
            note_char = "D#";
            break;
        case 7:
            note_char = "E-";
            break;
        case 8:
            note_char = "F-";
            break;
        case 9:
            note_char = "F#";
            break;
        case 10:
            note_char = "G-";
            break;
        case 11:
            note_char = "G#";
            break;
    }

    // Build string
    std::string s = "";
    s += note_char;
    s += octave_char;
    // Return string
    return s;
}

// Draw Phrase Editor UI
// off_y : UI offset on the y axis
void DrawPhraseUI(int off_y)
{
    // Width and height of the screen
    int h = phrase_grid_h;
    int w = phrase_grid_w;

    // Song position pointer
    if (((open_phrase_index == channellist[playing_channel].phrase_ptr &&
        open_chain_index == channellist[playing_channel].chain_ptr && open_channel == playing_channel) ||
        (channellist[open_channel].phrase_ptr == open_phrase_index && channellist[open_channel].chain_ptr == open_chain_index && 
        channellist[open_channel].cant_play == false && play_context == pt_song)
        ) &&
        pause_song == false)
    {
        geptr->DrawChar(4, 3 + channellist[playing_channel].step_ptr, geptr->entity, '>');
        geptr->DrawSetColor(4, 3 + channellist[playing_channel].step_ptr, geptr->entity, primary_text_a);
    }

    // Menu title and phrase number
    auto outstr = IntToHexString(open_phrase);
    outstr.insert(outstr.begin(), 4 - outstr.size(), '0');
    geptr->DrawTextString(0, 1, geptr->entity, "PHRASE - " + outstr, primary_text_a);
    
    // Draw the row numbers
    for (int i = 0; i < h && i + off_y < rowcount; i++)
    {
        int tempint = i + off_y;

        auto upperstr = IntToHexString(tempint);

        upperstr.insert(upperstr.begin(), 4 - upperstr.size(), '0');
        geptr->DrawTextString(0, 3 + i, geptr->entity, upperstr, primary_text_a);
    }

    // Channel headers
    geptr->DrawTextString(5, 2, geptr->entity, "NTE", header_text_a);
    geptr->DrawTextString(9, 2, geptr->entity, "ISTR", header_text_a);
    geptr->DrawTextString(14, 2, geptr->entity, "EFFT1", header_text_a);
    geptr->DrawTextString(20, 2, geptr->entity, "EFFT2", header_text_a);
    geptr->DrawTextString(26, 2, geptr->entity, "EFFT3", header_text_a);

    //Draw the phrase grid
    for (int y = 0; y < h && y + off_y < rowcount; y++)
    {
        // Note -
        auto thiscolor = cursor_y == y && cursor_x == 0 ? primary_text_b : primary_text_a;
            
        // Get the current phrase grid value
        int note = phraselist[open_phrase]->arr[y + off_y][0];
        if (note == -9999)
        {
            // Draw null note
            geptr->DrawTextString(5, 3 + y, geptr->entity, "---", thiscolor);
        }
        else
        {
            // Draw note
            geptr->DrawTextString(5, 3 + y, geptr->entity, IntToNoteString(note), thiscolor);
        }

        // Instrument -
        thiscolor = cursor_y == y && cursor_x == 1 ? primary_text_b : primary_text_a;

        // Get the current phrase grid value
        int instr = phraselist[open_phrase]->arr[y + off_y][1];
        if (instr == -1)
        {
            // Draw null instrument
            geptr->DrawTextString(9, 3 + y, geptr->entity, "----", thiscolor);
        }
        else
        {
            // Draw instrument number
            auto outstr = IntToHexString(instr);
            outstr.insert(outstr.begin(), 4 - outstr.size(), '0');
            geptr->DrawTextString(9, 3 + y, geptr->entity, outstr, thiscolor);
        }

        // Modify right part of the number
        if (leftrightcenter == left)
        {
            geptr->DrawSetColor(9, 3 + y, geptr->entity, primary_text_a);
            geptr->DrawSetColor(10, 3 + y, geptr->entity, primary_text_a);
        }
        // Modify left part of the number
        else if (leftrightcenter == right)
        {
            geptr->DrawSetColor(11, 3 + y, geptr->entity, primary_text_a);
            geptr->DrawSetColor(12, 3 + y, geptr->entity, primary_text_a);
        }

        // FX1 -
        thiscolor = cursor_y == y && cursor_x == 2 ? header_text_b : header_text_a;

        // Get the current phrase grid value
        int effect1 = phraselist[open_phrase]->arr[y + off_y][2];
        if (effect1 == -1)
        {
            // Draw null effect
            geptr->DrawTextString(14, 3 + y, geptr->entity, "-", thiscolor);
        }
        else
        {
            // Draw effect char
            std::string outchr = "";
            outchr += fx[effect1];
            geptr->DrawTextString(14, 3 + y, geptr->entity, outchr, thiscolor);
        }

        // FX1 Param -
        thiscolor = cursor_y == y && cursor_x == 3 ? primary_text_b : primary_text_a;

        // Get the current phrase grid value
        int effect1_param = phraselist[open_phrase]->arr[y + off_y][3];
        if (effect1_param == -1)
        {
            // Draw null parameter
            geptr->DrawTextString(15, 3 + y, geptr->entity, "----", thiscolor);
        }
        else
        {
            // Draw effect parameter
            auto outstr = IntToHexString(effect1_param);
            outstr.insert(outstr.begin(), 4 - outstr.size(), '0');
            geptr->DrawTextString(15, 3 + y, geptr->entity, outstr, thiscolor);
        }

        // Modify right part of the number
        if (leftrightcenter == left)
        {
            geptr->DrawSetColor(15, 3 + y, geptr->entity, primary_text_a);
            geptr->DrawSetColor(16, 3 + y, geptr->entity, primary_text_a);
        }
        // Modify left part of the number
        else if (leftrightcenter == right)
        {
            geptr->DrawSetColor(17, 3 + y, geptr->entity, primary_text_a);
            geptr->DrawSetColor(18, 3 + y, geptr->entity, primary_text_a);
        }

        // FX2 -
        thiscolor = cursor_y == y && cursor_x == 4 ? header_text_b : header_text_a;

        // Get the current phrase grid value
        int effect2 = phraselist[open_phrase]->arr[y + off_y][4];
        if (effect2 == -1)
        {
            // Draw null effect
            geptr->DrawTextString(20, 3 + y, geptr->entity, "-", thiscolor);
        }
        else
        {
            // Draw effect char
            std::string outchr = "";
            outchr += fx[effect2];
            geptr->DrawTextString(20, 3 + y, geptr->entity, outchr, thiscolor);
        }

        // FX2 Param -
        thiscolor = cursor_y == y && cursor_x == 5 ? primary_text_b : primary_text_a;

        // Get the current phrase grid value
        int effect2_param = phraselist[open_phrase]->arr[y + off_y][5];
        if (effect2_param == -1)
        {
            // Draw null parameter
            geptr->DrawTextString(21, 3 + y, geptr->entity, "----", thiscolor);
        }
        else
        {
            // Draw effect parameter
            auto outstr = IntToHexString(effect2_param);
            outstr.insert(outstr.begin(), 4 - outstr.size(), '0');
            geptr->DrawTextString(21, 3 + y, geptr->entity, outstr, thiscolor);
        }

        // Modify right part of the number
        if (leftrightcenter == left)
        {
            geptr->DrawSetColor(21, 3 + y, geptr->entity, primary_text_a);
            geptr->DrawSetColor(22, 3 + y, geptr->entity, primary_text_a);
        }
        // Modify left part of the number
        else if (leftrightcenter == right)
        {
            geptr->DrawSetColor(23, 3 + y, geptr->entity, primary_text_a);
            geptr->DrawSetColor(24, 3 + y, geptr->entity, primary_text_a);
        }

        // FX3 -
        thiscolor = cursor_y == y && cursor_x == 6 ? header_text_b : header_text_a;

        // Get the current phrase grid value
        int effect3 = phraselist[open_phrase]->arr[y + off_y][6];
        if (effect3 == -1)
        {
            // Draw null effect
            geptr->DrawTextString(26, 3 + y, geptr->entity, "-", thiscolor);
        }
        else
        {
            // Draw effect char
            std::string outchr = "";
            outchr += fx[effect3];
            geptr->DrawTextString(26, 3 + y, geptr->entity, outchr, thiscolor);
        }

        // FX3 Param -
        thiscolor = cursor_y == y && cursor_x == 7 ? primary_text_b : primary_text_a;

        // Get the current phrase grid value
        int effect3_param = phraselist[open_phrase]->arr[y + off_y][7];
        if (effect3_param == -1)
        {
            // Draw null parameter
            geptr->DrawTextString(27, 3 + y, geptr->entity, "----", thiscolor);
        }
        else
        {
            // Draw effect parameter
            auto outstr = IntToHexString(effect3_param);
            outstr.insert(outstr.begin(), 4 - outstr.size(), '0');
            geptr->DrawTextString(27, 3 + y, geptr->entity, outstr, thiscolor);
        }

        // Modify right part of the number
        if (leftrightcenter == left)
        {
            geptr->DrawSetColor(27, 3 + y, geptr->entity, primary_text_a);
            geptr->DrawSetColor(28, 3 + y, geptr->entity, primary_text_a);
        }
        // Modify left part of the number
        else if (leftrightcenter == right)
        {
            geptr->DrawSetColor(29, 3 + y, geptr->entity, primary_text_a);
            geptr->DrawSetColor(30, 3 + y, geptr->entity, primary_text_a);
        }
    }
}

// Draw Instrument Editor UI
void DrawInstrumentUI()
{
    // Menu title and instrument number
    auto outstr = IntToHexString(open_instrument);
    outstr.insert(outstr.begin(), 4 - outstr.size(), '0');
    geptr->DrawTextString(0, 1, geptr->entity, "INSTR - " + outstr, primary_text_a);

    // Editor body

    // Inits
    instrument_edit_y = -1;
    instrument_edit_digit_count = -1;

    // Draw instrument options
    int ty = 3;

    // Channel Type
    geptr->DrawTextString(0, ty, geptr->entity, "TYP:", primary_text_a); // Synth or Sample
    outstr = instrumentlist[open_instrument]->type == ChannelType::synth ? "SYNTH" : "FILE";
    geptr->DrawTextString(instrumentlist[open_instrument]->type == ChannelType::synth ? 5 : 7, ty, geptr->entity, outstr,
        cursor_y == ty - 3 && cursor_x == 0 ? primary_text_b : primary_text_a); // Show value
    if (cursor_y == ty - 3) { instrument_edit_y = ty; instrument_edit_digit_count = 0; }
    ty++;

    // Draw synth UI
    if (instrumentlist[open_instrument]->type == ChannelType::synth)
    {
        // Waveform
        geptr->DrawTextString(0, ty, geptr->entity, "WAV:", primary_text_a); // Sine, Square, Saw, etc.
        outstr = waveform_to_string[instrumentlist[open_instrument]->waveform];
        geptr->DrawTextString(5, ty, geptr->entity, outstr,
            cursor_y == ty - 3 && cursor_x == 0 ? primary_text_b : primary_text_a); // Show value
        if (cursor_y == ty - 3) { instrument_edit_y = ty; instrument_edit_digit_count = 0; }
        ty++;
        // Volume
        geptr->DrawTextString(0, ty, geptr->entity, "VOL:", primary_text_a); // Volume : 0x00 = 0, 0xFF = 1 | Fade : 0x80 = 0, 0x00 = -128, 0xFF = 127
        outstr = IntToHexString(instrumentlist[open_instrument]->volume_edit);
        outstr.insert(outstr.begin(), 4 - outstr.size(), '0');
        geptr->DrawTextString(5, ty, geptr->entity, outstr,
            cursor_y == ty - 3 && cursor_x == 0 ? primary_text_b : primary_text_a); // Show value
        if (cursor_y == ty - 3) { instrument_edit_y = ty; instrument_edit_digit_count = 2; }
        ty++;
        // Panning
        geptr->DrawTextString(0, ty, geptr->entity, "PAN:", primary_text_a); // Pan : 0x00 = 0, 0x80 = 0.5, 0xFF = 1 | Pan Mod : 0x00 = 1, 0xFF = X units per tick
        outstr = IntToHexString(instrumentlist[open_instrument]->pan_edit);
        outstr.insert(outstr.begin(), 4 - outstr.size(), '0');
        geptr->DrawTextString(5, ty, geptr->entity, outstr,
            cursor_y == ty - 3 && cursor_x == 0 ? primary_text_b : primary_text_a); // Show value
        if (cursor_y == ty - 3) { instrument_edit_y = ty; instrument_edit_digit_count = 2; }
        ty++;
        // Pulse width
        geptr->DrawTextString(0, ty, geptr->entity, "WID:", primary_text_a); // 0x00 = 0, 0x80 = 0.5, 0xFF = 1 | Pulse Width Mod : 0x00 = 1, 0xFF = X units per tick
        outstr = IntToHexString(instrumentlist[open_instrument]->pw_edit);
        outstr.insert(outstr.begin(), 4 - outstr.size(), '0');
        geptr->DrawTextString(5, ty, geptr->entity, outstr,
            cursor_y == ty - 3 && cursor_x == 0 ? primary_text_b : primary_text_a); // Show value
        if (cursor_y == ty - 3) { instrument_edit_y = ty; instrument_edit_digit_count = 2; }
        ty++;
        // Tuning
        geptr->DrawTextString(0, ty, geptr->entity, "TUN:", primary_text_a); // 0x80 = 0, 0x00 = -128, 0xFF = 127 semitones
        outstr = IntToHexString(instrumentlist[open_instrument]->detune_edit);
        outstr.insert(outstr.begin(), 2 - outstr.size(), '0');
        geptr->DrawTextString(5 + 2, ty, geptr->entity, outstr,
            cursor_y == ty - 3 && cursor_x == 0 ? primary_text_b : primary_text_a); // Show value
        if (cursor_y == ty - 3) { instrument_edit_y = ty; instrument_edit_digit_count = 1; }
        ty++;
        // Pitch sweep
        geptr->DrawTextString(0, ty, geptr->entity, "SWP:", primary_text_a); // 0x80 = 0, 0x00 = -32768, 0xFF = 32767 units per tick
        outstr = IntToHexString(instrumentlist[open_instrument]->pitch_freq_edit);
        outstr.insert(outstr.begin(), 4 - outstr.size(), '0');
        geptr->DrawTextString(5, ty, geptr->entity, outstr,
            cursor_y == ty - 3 && cursor_x == 0 ? primary_text_b : primary_text_a); // Show value
        if (cursor_y == ty - 3) { instrument_edit_y = ty; instrument_edit_digit_count = 2; }
        ty++;

        // Filter type
        ty++;
        geptr->DrawTextString(0, ty, geptr->entity, "FLT:", primary_text_a); // Lowpass, Bandpass, Highpass, None
        outstr = instrumentlist[open_instrument]->filter == FilterType::lowpass ? "LOWPASS" :
            instrumentlist[open_instrument]->filter == FilterType::bandpass ? "BANDPASS" :
            instrumentlist[open_instrument]->filter == FilterType::highpass ? "HIGHPASS" : "NONE";
        geptr->DrawTextString(5, ty, geptr->entity, outstr,
            cursor_y == ty - 4 && cursor_x == 0 ? primary_text_b : primary_text_a); // Show value
        if (cursor_y == ty - 4) { instrument_edit_y = ty; instrument_edit_digit_count = 0; }
        ty++;
        // Filter cutoff
        geptr->DrawTextString(0, ty, geptr->entity, "CTF:", primary_text_a); // Cutoff
        outstr = IntToHexString(instrumentlist[open_instrument]->cutoff_edit);
        outstr.insert(outstr.begin(), 4 - outstr.size(), '0');
        geptr->DrawTextString(5, ty, geptr->entity, outstr,
            cursor_y == ty - 4 && cursor_x == 0 ? primary_text_b : primary_text_a); // Show value
        if (cursor_y == ty - 4) { instrument_edit_y = ty; instrument_edit_digit_count = 2; }
        ty++;
        // Filter resonance
        geptr->DrawTextString(0, ty, geptr->entity, "RES:", primary_text_a); // Resonance
        outstr = IntToHexString(instrumentlist[open_instrument]->resonance_edit);
        outstr.insert(outstr.begin(), 4 - outstr.size(), '0');
        geptr->DrawTextString(5, ty, geptr->entity, outstr,
            cursor_y == ty - 4 && cursor_x == 0 ? primary_text_b : primary_text_a); // Show value
        if (cursor_y == ty - 4) { instrument_edit_y = ty; instrument_edit_digit_count = 2; }
        ty++;

        if (instrument_edit_y != -1 && instrument_edit_digit_count == 2)
        {
            // Modify left part of the number
            if (leftrightcenter == left)
            {
                geptr->DrawSetColor(5 + cursor_x, instrument_edit_y, geptr->entity, primary_text_a);
                geptr->DrawSetColor(6 + cursor_x, instrument_edit_y, geptr->entity, primary_text_a);
            }
            // Modify right part of the number
            if (leftrightcenter == right)
            {
                geptr->DrawSetColor(7 + cursor_x, instrument_edit_y, geptr->entity, primary_text_a);
                geptr->DrawSetColor(8 + cursor_x, instrument_edit_y, geptr->entity, primary_text_a);
            }
        }
    }
    else
    {
        // Sample index
        geptr->DrawTextString(0, ty, geptr->entity, "SPL:", primary_text_a); // File ref
        outstr = instrumentlist[open_instrument]->sample_index == -1 ? "----" : IntToHexString(instrumentlist[open_instrument]->sample_index);
        outstr.insert(outstr.begin(), 4 - outstr.size(), '0');
        geptr->DrawTextString(5+2, ty, geptr->entity, outstr,
            cursor_y == ty - 3 && cursor_x == 0 ? primary_text_b : primary_text_a); // Show value
        if (cursor_y == ty - 3) { instrument_edit_y = ty; instrument_edit_digit_count = 2; }
        ty++;
        // Volume
        geptr->DrawTextString(0, ty, geptr->entity, "VOL:", primary_text_a); // Volume : 0x00 = 0, 0xFF = 1 | Fade : 0x80 = 0, 0x00 = -128, 0xFF = 127
        outstr = IntToHexString(instrumentlist[open_instrument]->volume_edit);
        outstr.insert(outstr.begin(), 4 - outstr.size(), '0');
        geptr->DrawTextString(5 + 2, ty, geptr->entity, outstr,
            cursor_y == ty - 3 && cursor_x == 0 ? primary_text_b : primary_text_a); // Show value
        if (cursor_y == ty - 3) { instrument_edit_y = ty; instrument_edit_digit_count = 2; }
        ty++;
        // Panning
        geptr->DrawTextString(0, ty, geptr->entity, "PAN:", primary_text_a); // Pan : 0x00 = 0, 0x80 = 0.5, 0xFF = 1 | Pan Mod : 0x00 = 1, 0xFF = X units per tick
        outstr = IntToHexString(instrumentlist[open_instrument]->pan_edit);
        outstr.insert(outstr.begin(), 4 - outstr.size(), '0');
        geptr->DrawTextString(5 + 2, ty, geptr->entity, outstr,
            cursor_y == ty - 3 && cursor_x == 0 ? primary_text_b : primary_text_a); // Show value
        if (cursor_y == ty - 3) { instrument_edit_y = ty; instrument_edit_digit_count = 2; }
        ty++;
        // Tuning
        geptr->DrawTextString(0, ty, geptr->entity, "TUN:", primary_text_a); // 0x80 = 0, 0x00 = -128, 0xFF = 127 semitones
        outstr = IntToHexString(instrumentlist[open_instrument]->detune_edit);
        outstr.insert(outstr.begin(), 2 - outstr.size(), '0');
        geptr->DrawTextString(5 + 4, ty, geptr->entity, outstr,
            cursor_y == ty - 3 && cursor_x == 0 ? primary_text_b : primary_text_a); // Show value
        if (cursor_y == ty - 3) { instrument_edit_y = ty; instrument_edit_digit_count = 1; }
        ty++;
        // Pitch sweep
        geptr->DrawTextString(0, ty, geptr->entity, "SWP:", primary_text_a); // 0x80 = 0, 0x00 = -32768, 0xFF = 32767 units per tick
        outstr = IntToHexString(instrumentlist[open_instrument]->pitch_freq_edit);
        outstr.insert(outstr.begin(), 4 - outstr.size(), '0');
        geptr->DrawTextString(5 + 2, ty, geptr->entity, outstr,
            cursor_y == ty - 3 && cursor_x == 0 ? primary_text_b : primary_text_a); // Show value
        if (cursor_y == ty - 3) { instrument_edit_y = ty; instrument_edit_digit_count = 2; }
        ty++;
        // Base pitch
        geptr->DrawTextString(0, ty, geptr->entity, "BSP:", primary_text_a); // 39 = C-4
        outstr = IntToNoteString(instrumentlist[open_instrument]->base_pitch);
        geptr->DrawTextString(5 + 3, ty, geptr->entity, outstr,
            cursor_y == ty - 3 && cursor_x == 0 ? primary_text_b : primary_text_a); // Show value
        if (cursor_y == ty - 3) { instrument_edit_y = ty; instrument_edit_digit_count = 1.5; }
        ty++;
        // Start time
        geptr->DrawTextString(0, ty, geptr->entity, "BEG:", primary_text_a); // 0x000000 = 0 ms
        outstr = IntToHexString(instrumentlist[open_instrument]->start_time_ms);
        outstr.insert(outstr.begin(), 6 - outstr.size(), '0');
        geptr->DrawTextString(5, ty, geptr->entity, outstr,
            cursor_y == ty - 3 && cursor_x == 0 ? primary_text_b : primary_text_a); // Show value
        if (cursor_y == ty - 3) { instrument_edit_y = ty; instrument_edit_digit_count = 3; }
        ty++;
        // mid time
        geptr->DrawTextString(0, ty, geptr->entity, "MID:", primary_text_a); // 0x000000 = 0 ms
        outstr = IntToHexString(instrumentlist[open_instrument]->mid_time_ms);
        outstr.insert(outstr.begin(), 6 - outstr.size(), '0');
        geptr->DrawTextString(5, ty, geptr->entity, outstr,
            cursor_y == ty - 3 && cursor_x == 0 ? primary_text_b : primary_text_a); // Show value
        if (cursor_y == ty - 3) { instrument_edit_y = ty; instrument_edit_digit_count = 3; }
        ty++;
        // End time
        geptr->DrawTextString(0, ty, geptr->entity, "END:", primary_text_a); // 0xFFFFFF = 16777215 ms
        outstr = IntToHexString(instrumentlist[open_instrument]->end_time_ms);
        outstr.insert(outstr.begin(), 6 - outstr.size(), '0');
        geptr->DrawTextString(5, ty, geptr->entity, outstr,
            cursor_y == ty - 3 && cursor_x == 0 ? primary_text_b : primary_text_a); // Show value
        if (cursor_y == ty - 3) { instrument_edit_y = ty; instrument_edit_digit_count = 3; }
        ty++;
        // Loop
        geptr->DrawTextString(0, ty, geptr->entity, "LOP:", primary_text_a); // TRUE, FALSE
        outstr = instrumentlist[open_instrument]->loop == 0 ? "FALSE" : "TRUE";
        geptr->DrawTextString(6 + (int)instrumentlist[open_instrument]->loop, ty, geptr->entity, outstr,
            cursor_y == ty - 3 && cursor_x == 0 ? primary_text_b : primary_text_a); // Show value
        if (cursor_y == ty - 3) { instrument_edit_y = ty; instrument_edit_digit_count = 4; }
        ty++;
        

        // Filter type
        ty++;
        geptr->DrawTextString(0, ty, geptr->entity, "FLT:", primary_text_a); // Lowpass, Bandpass, Highpass, None
        outstr = instrumentlist[open_instrument]->filter == FilterType::lowpass ? "LOWPASS" :
            instrumentlist[open_instrument]->filter == FilterType::bandpass ? "BANDPASS" :
            instrumentlist[open_instrument]->filter == FilterType::highpass ? "HIGHPASS" : "NONE";
        geptr->DrawTextString(5 + 2, ty, geptr->entity, outstr,
            cursor_y == ty - 4 && cursor_x == 0 ? primary_text_b : primary_text_a); // Show value
        if (cursor_y == ty - 4) { instrument_edit_y = ty; instrument_edit_digit_count = 0; }
        ty++;
        // Filter cutoff
        geptr->DrawTextString(0, ty, geptr->entity, "CTF:", primary_text_a); // Cutoff
        outstr = IntToHexString(instrumentlist[open_instrument]->cutoff_edit);
        outstr.insert(outstr.begin(), 4 - outstr.size(), '0');
        geptr->DrawTextString(5 + 2, ty, geptr->entity, outstr,
            cursor_y == ty - 4 && cursor_x == 0 ? primary_text_b : primary_text_a); // Show value
        if (cursor_y == ty - 4) { instrument_edit_y = ty; instrument_edit_digit_count = 2; }
        ty++;
        // Filter resonance
        geptr->DrawTextString(0, ty, geptr->entity, "RES:", primary_text_a); // Resonance
        outstr = IntToHexString(instrumentlist[open_instrument]->resonance_edit);
        outstr.insert(outstr.begin(), 4 - outstr.size(), '0');
        geptr->DrawTextString(5 + 2, ty, geptr->entity, outstr,
            cursor_y == ty - 4 && cursor_x == 0 ? primary_text_b : primary_text_a); // Show value
        if (cursor_y == ty - 4) { instrument_edit_y = ty; instrument_edit_digit_count = 2; }
        ty++;

        if (instrument_edit_y != -1 && instrument_edit_digit_count != 4 && instrument_edit_digit_count >= 2)
        {
            int offset = 0;
            if (alt_mode)
                offset = -2;
            // Modify left part of the number
            if (leftrightcenter == left)
            {
                geptr->DrawSetColor(5 + 2 + cursor_x + offset, instrument_edit_y, geptr->entity, primary_text_a);
                geptr->DrawSetColor(6 + 2 + cursor_x + offset, instrument_edit_y, geptr->entity, primary_text_a);
            }
            // Modify right part of the number
            if (leftrightcenter == right)
            {
                geptr->DrawSetColor(7 + 2 + cursor_x + offset, instrument_edit_y, geptr->entity, primary_text_a);
                geptr->DrawSetColor(8 + 2 + cursor_x + offset, instrument_edit_y, geptr->entity, primary_text_a);
            }
        }
    }
}

// Draw Instrument Editor UI
void DrawWaveUI()
{
    // Synth or File instrument?
    if (instrumentlist[open_instrument]->type == ChannelType::synth)
    {
        // TODO: Implement UI for waveform editing
    }
    else
    {
        // Menu title and sample number
        auto outstr = IntToHexString(open_sample);
        outstr.insert(outstr.begin(), 4 - outstr.size(), '0');
        geptr->DrawTextString(0, 1, geptr->entity, "SMPLE - " + outstr, primary_text_a);
        
        // Draw current directory -

        // Create sample library if it does not exist
        if (!std::filesystem::exists(current_dir))
            std::filesystem::create_directory(current_dir);

        // Loop through contents and draw them
        int i = 0;
        std::list<std::filesystem::directory_entry> files;

        // Get all the objects in the archive
        for (const auto& entry : std::filesystem::directory_iterator(current_dir))
        {
            files.push_front(entry);
        }

        // Get length of the directory
        current_dir_length = files.size();
        
        // Sort the folder and file list
        files.sort([](const std::filesystem::directory_entry& a, const std::filesystem::directory_entry& b)
            {
                auto patha = a.path().string();
                auto pathb = b.path().string();
                int at = 0;
                for (auto c : patha)
                    patha[at++] = (char)toupper(c);
                at = 0;
                for (auto c : pathb)
                    pathb[at++] = (char)toupper(c);
                return !(a.is_regular_file() && b.is_directory()) && patha < pathb;
            }
        );

        // Draw them
        for (auto entry : files)
        {
            // Skip until we get to the offset
            if (i < sample_offset_y)
            {
                i++;
                continue;
            }

            // Get the highlighted path
            if (i - sample_offset_y == cursor_y)
            {
                selected_path = entry.path().string();
                selected_path_isdir = entry.is_directory();
                if (selected_path_isdir)
                    selected_path += "\\";
            }

            // Draw the entry
            auto path = entry.path().string();
            int at = 0;
            int l = current_dir.length();
            int r = path.length() - current_dir.length();
            path = path.substr(l, r);
            for (auto c : path)
                path[at++] = (char)toupper(c);
            if (entry.is_regular_file())
                geptr->DrawTextString(1, 3 + i - sample_offset_y, geptr->entity, path, i - sample_offset_y == cursor_y ? primary_text_b : primary_text_a);
            else
                geptr->DrawTextString(1, 3 + i - sample_offset_y, geptr->entity, path, i - sample_offset_y == cursor_y ? header_text_b : header_text_a);
            i++;
            
            // Have we displayed the max
            if (i >= file_display_count + sample_offset_y)
                break;
        }

        // Draw loaded header and current file text
        geptr->DrawTextString(1, 3 + file_display_count + 1, geptr->entity, "LOADED:", header_text_a);
        geptr->DrawTextString(1, 3 + file_display_count + 2, geptr->entity, samplelist[open_sample]->path, primary_text_a);
    }
}

// Draw the map for where you are in the UI
void DrawMap()
{
    // Draw the map
    geptr->DrawChar(geptr->GetCanvasW() - 1, geptr->GetCanvasH() - 1, geptr->entity, 'T'); // Table
    geptr->DrawSetColor(geptr->GetCanvasW() - 1, geptr->GetCanvasH() - 1, geptr->entity, state == m_table ? header_text_b : header_text_a);
    geptr->DrawChar(geptr->GetCanvasW() - 1, geptr->GetCanvasH() - 2, geptr->entity, 'I'); // Instrument
    geptr->DrawSetColor(geptr->GetCanvasW() - 1, geptr->GetCanvasH() - 2, geptr->entity, state == m_instrument ? header_text_b : header_text_a);
    geptr->DrawChar(geptr->GetCanvasW() - 1, geptr->GetCanvasH() - 3, geptr->entity, 'W'); // Wave
    geptr->DrawSetColor(geptr->GetCanvasW() - 1, geptr->GetCanvasH() - 3, geptr->entity, state == m_wave ? header_text_b : header_text_a);
    geptr->DrawChar(geptr->GetCanvasW() - 2, geptr->GetCanvasH() - 2, geptr->entity, 'P'); // Phrase
    geptr->DrawSetColor(geptr->GetCanvasW() - 2, geptr->GetCanvasH() - 2, geptr->entity, state == m_phrase ? header_text_b : header_text_a);
    geptr->DrawChar(geptr->GetCanvasW() - 3, geptr->GetCanvasH() - 2, geptr->entity, 'C'); // Chain
    geptr->DrawSetColor(geptr->GetCanvasW() - 3, geptr->GetCanvasH() - 2, geptr->entity, state == m_chain ? header_text_b : header_text_a);
    geptr->DrawChar(geptr->GetCanvasW() - 4, geptr->GetCanvasH() - 2, geptr->entity, 'S'); // Song
    geptr->DrawSetColor(geptr->GetCanvasW() - 4, geptr->GetCanvasH() - 2, geptr->entity, state == m_song ? header_text_b : header_text_a);
    geptr->DrawChar(geptr->GetCanvasW() - 4, geptr->GetCanvasH() - 3, geptr->entity, 'O'); // Options
    geptr->DrawSetColor(geptr->GetCanvasW() - 4, geptr->GetCanvasH() - 3, geptr->entity, state == m_options ? header_text_b : header_text_a);
}

// Handle tick hitting - This should be called from a separate thread
void TrackTicks()
{
    // The next time is defined by starting at the current time
    auto next = std::chrono::steady_clock::now();

    // Mark that the thread is running
    play_thread = true;

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

			// TODO: Test using thread sleep instead of sdl delay

            // Should we sleep or nah?
            if (rem > std::chrono::milliseconds(5))
                SDL_Delay(1); // Sleep the thread to relieve the CPU
            else
            {
                /* Spin in place until the clock hits the next frame */
            }        
        }
    }

    // Mark that the thread is not running
    play_thread = false;
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

// Stop all audio playback
void StopAllChannels()
{
    for (int i = 0; i < channelcount; i++)
        geptr->StopChannel(i);
}

// Handle deep copy inputs
void GetDeepCopyInputs()
{
    // Handle copy
    if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_select_down())
    {
        // If a is pressed after b, then increase
        if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_a_pressed() && do_deep_copy == 1)
        {
            do_deep_copy = 2;
        }
        // If b is pressed again, reset
        if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_b_pressed() && do_deep_copy == 1)
        {
            do_deep_copy = 0;
        }
        // If b is pressed start checking for deep copy
        if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_b_pressed() && do_deep_copy == 0)
        {
            do_deep_copy = 1;
        }
        // If anything else is pressed, reset
        if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->presscode != "000000000" &&
            dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->presscode != "000010000" &&
            dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->presscode != "000001000")
            do_deep_copy = 0;
    }
    else
    {
        // Reset deep copy tracker
        do_deep_copy = 0;
    }
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
    bool willpause = false;

    // Handle play button
    if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_start_pressed() && pause_song == false)
        // Pause the song playback
        willpause = true;

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
        // Handle deep copy input logic
        GetDeepCopyInputs();

        // If we have a successful deep copy, do it
        if (do_deep_copy == 2 && GetAt(&chainlist, songgrid[cursor_y + offset_y][cursor_x + offset_x]) != nullptr)
        {
            // Deep copy the phrase and replace it here in the chain
            songgrid[cursor_y + offset_y][cursor_x + offset_x] = DeepCopyChain(songgrid[cursor_y + offset_y][cursor_x + offset_x]);
        }
        else
        {
            // Handle play button
            if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_start_pressed() && pause_song == true)
            {
                StopAllChannels();
                // Should play flag
                bool dont_play = true;
                // Set the song ptr position for only this channel
                for (int i = 0; i < channelcount; i++)
                    dont_play = channellist[i].init_song_play(cursor_y + offset_y) && dont_play;
                // Should we play or not?
                if (dont_play == false)
                {
                    // Set the scope of play to only this phrase
                    play_context = pt_song;
                    // Unpause the song playback
                    pause_song = false;
                }
            }

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
            // Goto page
            else if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_select_down())
            {
                // Go to the next page over
                if (goright)
                {
                    // Set open audio channel
                    open_channel = cursor_x + offset_x;
                    // Set open chain
                    if (songgrid[cursor_y + offset_y][cursor_x + offset_x] != -1)
                    {
                        open_chain = songgrid[cursor_y + offset_y][cursor_x + offset_x];
                        // Set open chain index
                        open_chain_index = cursor_y + offset_y;
                    }
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
                        cursor_x = SDL_clamp(cursor_x, 0, chain_grid_w-1);
                        cursor_y = SDL_clamp(cursor_y, 0, chain_grid_h-1);
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
        }

        // Reset deep copy action flag
        if (do_deep_copy == 2)
            do_deep_copy = 0;

        // Update UI
        DrawSongUI(offset_x, offset_y);
    }

    // Handle input for the chain menu
    if (state == m_chain && !breakend)
    {
        // Handle deep copy input logic
        GetDeepCopyInputs();

        // If we have a successful deep copy, do it
        if (do_deep_copy == 2 && GetAt(&phraselist, chainlist[open_chain]->arr[cursor_y + chain_offset_y]) != nullptr)
        {
            // Deep copy the phrase and replace it here in the chain
            chainlist[open_chain]->arr[cursor_y + chain_offset_y] = DeepCopyPhrase(chainlist[open_chain]->arr[cursor_y + chain_offset_y]);
        }
        else
        {
            // Handle play button
            if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_start_pressed() && pause_song == true)
            {
                StopAllChannels();
                // Set the song ptr position for only this channel
                playing_channel = open_channel;
                channellist[playing_channel].chain_ptr = open_chain_index;
                channellist[playing_channel].playing_chain = open_chain;
                channellist[playing_channel].phrase_ptr = cursor_y + chain_offset_y;
                channellist[playing_channel].playing_phrase = -1;
                channellist[playing_channel].step_ptr = 0;
                channellist[playing_channel].tick_ptr = 0;
                // Set the scope of play to only this phrase
                play_context = pt_chain;
                // Unpause the song playback
                pause_song = false;
            }

            // Modify value
            if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_a_pressed())
            {
                // Edit actual phrase
                if (cursor_x == 0)
                {
                    // If the chainlist value is unfilled, insert 0
                    if (chainlist[open_chain]->arr[cursor_y + chain_offset_y] == -1)
                    {
                        if (copied_phrase == -1)
                        {
                            // Set UI reference to Hex0
                            chainlist[open_chain]->arr[cursor_y + chain_offset_y] = 0x0000;
                            // If this phrase doesn't exist yet, insert it
                            if (GetAt(&phraselist, chainlist[open_chain]->arr[cursor_y + chain_offset_y]) == nullptr)
                                InsertAt(&phraselist, chainlist[open_chain]->arr[cursor_y + chain_offset_y], new phrase());
                        }
                        else
                        {
                            // Set UI reference to copied chain
                            chainlist[open_chain]->arr[cursor_y + chain_offset_y] = copied_phrase;
                        }
                    }
                    // If double click, add a new chain
                    else if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->doubleclick)
                    {
                        // Set UI reference to the next empty
                        chainlist[open_chain]->arr[cursor_y + chain_offset_y] = GetNextEmpty(&phraselist);
                        // Add the new phrase
                        InsertAt(&phraselist, chainlist[open_chain]->arr[cursor_y + chain_offset_y], new phrase());
                        // Copy to clipboard
                        copied_phrase = chainlist[open_chain]->arr[cursor_y + chain_offset_y];
                    }
                    else
                    {
                        // Copy to clipboard
                        copied_phrase = chainlist[open_chain]->arr[cursor_y + chain_offset_y];
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
            // Goto page
            else if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_select_down())
            {
                // Go to the left page over
                if (goleft)
                {
                    // Song
                    state = m_song;
                    cursor_x = SDL_clamp(cursor_x, 0, song_grid_w - 1);
                    cursor_y = SDL_clamp(cursor_y, 0, song_grid_h - 1);
                    breakend = true;
                }
                // Go to the right page over
                else if (goright)
                {
                    // Set open phrase
                    if (chainlist[open_chain]->arr[cursor_y + chain_offset_y] != -1)
                    {
                        open_phrase = chainlist[open_chain]->arr[cursor_y + chain_offset_y];
                        // Set open phrase index
                        open_phrase_index = cursor_y + chain_offset_y;
                    }
                    // If the open_chain is valid
                    if (open_phrase != -1)
                    {
                        // Check if the phrase does not exist
                        if (GetAt(&phraselist, open_phrase) == nullptr)
                        {
                            InsertAt(&phraselist, open_phrase, new phrase());
                        }
                        // Chain
                        state = m_phrase;
                        cursor_x = SDL_clamp(cursor_x, 0, phrase_grid_w - 1);
                        cursor_y = SDL_clamp(cursor_y, 0, phrase_grid_h - 1);
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
        }

        // Reset deep copy action flag
        if (do_deep_copy == 2)
            do_deep_copy = 0;

        // Update UI
        DrawChainUI(chain_offset_y);
    }

    // Handle input for the phrase menu
    if (state == m_phrase && !breakend)
    {
        // Handle deep copy input logic
        GetDeepCopyInputs();

        // If we have a successful deep copy, do it
        if (do_deep_copy == 2 && cursor_x == 1 && GetAt(&instrumentlist, phraselist[open_phrase]->arr[cursor_y + chain_offset_y][cursor_x]) != nullptr)
        {
            // Deep copy the instrument
            auto i = GetAt(&instrumentlist, phraselist[open_phrase]->arr[cursor_y + chain_offset_y][cursor_x])->DeepCopyInstrument();
            auto index = GetNextEmpty(&instrumentlist);
            InsertAt(&instrumentlist, index, i);
            phraselist[open_phrase]->arr[cursor_y + chain_offset_y][cursor_x] = index;
        }
        else
        {
            // Handle play button
            if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_start_pressed() && pause_song == true)
            {
                StopAllChannels();
                // Set the song ptr position for only this channel
                playing_channel = open_channel;
                channellist[playing_channel].chain_ptr = open_chain_index;
                channellist[playing_channel].playing_chain = open_chain;
                channellist[playing_channel].phrase_ptr = open_phrase_index;
                channellist[playing_channel].playing_phrase = open_phrase;
                channellist[playing_channel].step_ptr = 0;
                channellist[playing_channel].tick_ptr = 0;
                // Set the scope of play to only this phrase
                play_context = pt_phrase;
                // Unpause the song playback
                pause_song = false;
            }

            // Modify value
            if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_a_pressed())
            {
                // Edit instr
                if (cursor_x == 1)
                {
                    // If the phraselist value is unfilled, insert 0
                    if (phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][cursor_x] == -1)
                    {
                        if (copied_instr == -1)
                        {
                            // Set UI reference to Hex0
                            phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][cursor_x] = 0x0000;
                            // If this instrument doesn't exist yet, insert it
                            if (GetAt(&instrumentlist, phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][cursor_x]) == nullptr)
                                InsertAt(&instrumentlist, phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][cursor_x], new instrument());
                        }
                        else
                        {
                            // Set UI reference to copied instrument
                            phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][cursor_x] = copied_instr;
                        }
                    }
                    // If double click, add a new instrument
                    else if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->doubleclick)
                    {
                        // Set UI reference to the next empty
                        phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][cursor_x] = GetNextEmpty(&instrumentlist);
                        // Add the new instrument
                        InsertAt(&instrumentlist, phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][cursor_x], new instrument());
                        // Copy to clipboard
                        copied_instr = phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][cursor_x];
                    }
                    else
                    {
                        // Copy to clipboard
                        copied_instr = phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][cursor_x];
                    }
                }
            }

            // Do actions given the context --

            // Editing
            if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_a_down())
            {
                // Edit note
                if (cursor_x == 0)
                {
                    // If the phraselist instrument value is unfilled, insert 0
                    if (phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][1] == -1 && dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_a_pressed())
                    {
                        if (copied_instr == -1)
                        {
                            // Set UI reference to Hex0
                            phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][1] = 0x0000;
                            // If this instrument doesn't exist yet, insert it
                            if (GetAt(&instrumentlist, phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][1]) == nullptr)
                                InsertAt(&instrumentlist, phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][1], new instrument());
                        }
                        else
                        {
                            // Set UI reference to copied instrument
                            phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][1] = copied_instr;
                        }
                    }

                    // Set to C-4 if it isn't set
                    if (phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][0] == -9999 && dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_a_pressed())
                        phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][0] = copied_note != -9999 ? copied_note : 39;

                    // Movement keys
                    if (goup || godown || goright || goleft)
                    {
                        // Edit note pitch
                        if (goup) phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][0] += 12;
                        if (godown) phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][0] -= 12;
                        if (goright) phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][0] += 1;
                        if (goleft) phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][0] -= 1;

                        // Wrap the cell between min_note and max_note
                        if (phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][0] < min_note)
                            phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][0] = max_note + ((phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][0] - min_note) + 1);
                        if (phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][0] > max_note)
                            phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][0] = min_note + (phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][0] - 1) - max_note;
                    }

                    // Copy to clipboard
                    copied_note = phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][0];

                    // Preview note
                    if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_a_pressed() || goup || godown || goright || goleft)
                        PlayStepPhrase(open_channel, open_phrase, cursor_y + phrase_offset_y);

                    // Handle deletes
                    if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_b_pressed())
                    {
                        phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][1] = -1;
                        phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][0] = -9999;
                    }
                }

                // Edit instr
                if (cursor_x == 1)
                {
                    // Movement keys
                    if (goup || godown || goright || goleft)
                    {
                        // Mod the left two digits
                        if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_shift_down())
                        {
                            if (goup) phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][cursor_x] += 0x1000;
                            if (godown) phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][cursor_x] -= 0x1000;
                            if (goright) phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][cursor_x] += 0x0100;
                            if (goleft) phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][cursor_x] -= 0x0100;
                        }
                        // Mod the right two digits
                        else
                        {
                            if (goup) phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][cursor_x] += 0x0010;
                            if (godown) phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][cursor_x] -= 0x0010;
                            if (goright) phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][cursor_x] += 0x0001;
                            if (goleft) phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][cursor_x] -= 0x0001;
                        }

                        // Wrap the cell between 0x0000 and 0xFFFF
                        if (phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][cursor_x] < 0)
                            phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][cursor_x] = 0xFFFF + (phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][cursor_x] + 1);
                        if (phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][cursor_x] > 0xFFFF)
                            phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][cursor_x] = (phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][cursor_x] - 1) - 0xFFFF;

                        // Copy to clipboard
                        copied_instr = phraselist[open_phrase]->arr[cursor_y + offset_y][cursor_x + offset_x];
                    }

                    // Handle deletes
                    if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_b_pressed())
                        phraselist[open_phrase]->arr[cursor_y + offset_y][cursor_x + offset_x] = -1;
                }

                // Edit fx1,2,3
                if (cursor_x == 2 || cursor_x == 4 || cursor_x == 6)
                {
                    // Set to 0 if it isn't set
                    if (phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][cursor_x] == -1 && dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_a_pressed())
                        phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][cursor_x] = copied_effect != -1 ? copied_effect : 0;

                    // Movement keys
                    if (goup || godown || goright || goleft)
                    {
                        if (goup) phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][cursor_x] += 5;
                        if (godown) phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][cursor_x] -= 5;
                        if (goright) phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][cursor_x] += 1;
                        if (goleft) phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][cursor_x] -= 1;

                        // Wrap the cell between 0 and effect list length
                        int effect_list_len = (sizeof(fx) / sizeof(fx[0])) - 1;
                        if (phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][cursor_x] < 0)
                            phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][cursor_x] = effect_list_len + (phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][cursor_x] + 1);
                        if (phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][cursor_x] > effect_list_len)
                            phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][cursor_x] = (phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][cursor_x] - 1) - effect_list_len;
                    }

                    // Copy to clipboard
                    copied_effect = phraselist[open_phrase]->arr[cursor_y + offset_y][cursor_x + offset_x];

                    // Handle deletes
                    if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_b_pressed())
                        phraselist[open_phrase]->arr[cursor_y + offset_y][cursor_x + offset_x] = -1;
                }

                // Edit fx1,2,3 param
                if (cursor_x == 3 || cursor_x == 5 || cursor_x == 7)
                {
                    // Set to 0 if it isn't set
                    if (phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][cursor_x] == -1 && dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_a_pressed())
                        phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][cursor_x] = copied_effect_param != -1 ? copied_effect_param : 0x0000;

                    // Movement keys
                    if (goup || godown || goright || goleft)
                    {
                        // Mod the left two digits
                        if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_shift_down())
                        {
                            if (goup) phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][cursor_x] += 0x1000;
                            if (godown) phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][cursor_x] -= 0x1000;
                            if (goright) phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][cursor_x] += 0x0100;
                            if (goleft) phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][cursor_x] -= 0x0100;
                        }
                        // Mod the right two digits
                        else
                        {
                            if (goup) phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][cursor_x] += 0x0010;
                            if (godown) phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][cursor_x] -= 0x0010;
                            if (goright) phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][cursor_x] += 0x0001;
                            if (goleft) phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][cursor_x] -= 0x0001;
                        }

                        // Wrap the cell between 0x0000 and 0xFFFF
                        if (phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][cursor_x] < 0)
                            phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][cursor_x] = 0xFFFF + (phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][cursor_x] + 1);
                        if (phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][cursor_x] > 0xFFFF)
                            phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][cursor_x] = (phraselist[open_phrase]->arr[cursor_y + phrase_offset_y][cursor_x] - 1) - 0xFFFF;
                    }

                    // Copy to clipboard
                    copied_effect_param = phraselist[open_phrase]->arr[cursor_y + offset_y][cursor_x + offset_x];

                    // Handle deletes
                    if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_b_pressed())
                        phraselist[open_phrase]->arr[cursor_y + offset_y][cursor_x + offset_x] = -1;
                }
            }
            // Goto page
            else if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_select_down())
            {
                // Go to the left page over
                if (goleft)
                {
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
                        cursor_x = SDL_clamp(cursor_x, 0, chain_grid_w - 1);
                        cursor_y = SDL_clamp(cursor_y, 0, chain_grid_h - 1);
                        breakend = true;
                    }
                }
                // Go to the right page over
                else if (goright)
                {
                    // Set open instrument
                    if (phraselist[open_phrase]->arr[cursor_y + chain_offset_y][1] != -1)
                    {
                        open_instrument = phraselist[open_phrase]->arr[cursor_y + chain_offset_y][1];
                    }
                    // If the open_instrument is valid
                    if (open_instrument != -1)
                    {
                        // Check if the instrument does not exist
                        if (GetAt(&instrumentlist, open_instrument) == nullptr)
                        {
                            InsertAt(&instrumentlist, open_instrument, new instrument());
                        }
                        // Chain
                        state = m_instrument;
                        cursor_x = SDL_clamp(cursor_x, 0, GetAt(&instrumentlist, open_instrument)->type == ChannelType::synth ? instrument::synth_menu_width - 1 : instrument::sample_menu_width - 1);
                        cursor_y = SDL_clamp(cursor_y, 0, GetAt(&instrumentlist, open_instrument)->type == ChannelType::synth ? instrument::synth_menu_height - 1 : instrument::sample_menu_height - 1);
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

            // Stop previewing
            if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_a_released())
            {
                StopAllChannels();
            }

            // Next and last phrase
            auto next_phrase_index = (open_phrase_index + 1) % chain::length;
            auto next_phrase = GetAt(&phraselist, chainlist[open_chain]->arr[next_phrase_index]);
            auto last_phrase_index = (open_phrase_index - 1 < 0 ? chain::length - 1 : open_phrase_index - 1) % chain::length;
            auto last_phrase = GetAt(&phraselist, chainlist[open_chain]->arr[last_phrase_index]);

            // wrap the cursor and clamp offsets
            if (cursor_x > phrase_grid_w - 1)
                cursor_x = 0;
            if (cursor_x < 0)
                cursor_x = phrase_grid_w - 1;
            if (cursor_y + phrase_offset_y > phrase::len_y - 1 && next_phrase != nullptr)
            {
                cursor_y = 0;
                phrase_offset_y = 0;
                open_phrase = chainlist[open_chain]->arr[next_phrase_index];
                open_phrase_index = next_phrase_index;
            }
            if (cursor_y + phrase_offset_y < 0 && last_phrase != nullptr)
            {
                cursor_y = phrase_grid_h - 1;
                phrase_offset_y = phrase::len_y - phrase_grid_h;
                open_phrase = chainlist[open_chain]->arr[last_phrase_index];
                open_phrase_index = last_phrase_index;
            }

            // Move the page
            if (cursor_y > phrase_grid_h - 1)
                phrase_offset_y += 1;
            if (cursor_y < 0)
                phrase_offset_y -= 1;

            cursor_y = SDL_clamp(cursor_y, 0, phrase_grid_h - 1);
            phrase_offset_y = SDL_clamp(phrase_offset_y, 0, phrase::len_y - phrase_grid_h);
        }

        // Reset deep copy action flag
        if (do_deep_copy == 2)
            do_deep_copy = 0;

        // Update UI
        DrawPhraseUI(phrase_offset_y);
    }

    // Handle input for the instrument menu
    if (state == m_instrument && !breakend)
    {
        // For editing synth instrument
        if (instrumentlist[open_instrument]->type == ChannelType::synth)
        {
            // Editing
            if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_a_down())
            {
                // Get edit ptr
                int* edit;

                // Default
                edit = &(instrumentlist[open_instrument]->volume_edit);

                // Get instrument edit pointer
                switch (instrument_edit_y)
                {
                case 3: // Channel Type
                    edit = reinterpret_cast<int*>(&instrumentlist[open_instrument]->type);
                    break;
                case 4: // Waveform
                    edit = reinterpret_cast<int*>(&instrumentlist[open_instrument]->waveform);
                    break;
                case 5: // Volume
                    edit = &(instrumentlist[open_instrument]->volume_edit);
                    break;
                case 6: // Panning
                    edit = &(instrumentlist[open_instrument]->pan_edit);
                    break;
                case 7: // Pulse Width
                    edit = &(instrumentlist[open_instrument]->pw_edit);
                    break;
                case 8: // Tuning
                    edit = &(instrumentlist[open_instrument]->detune_edit);
                    break;
                case 9: // Pitch Sweep
                    edit = &(instrumentlist[open_instrument]->pitch_freq_edit);
                    break;
                case 11: // Filter Type
                    edit = reinterpret_cast<int*>(&instrumentlist[open_instrument]->filter);
                    break;
                case 12: // Filter Cutoff
                    edit = &(instrumentlist[open_instrument]->cutoff_edit);
                    break;
                case 13: // Filter Resonance
                    edit = &(instrumentlist[open_instrument]->resonance_edit);
                    break;
                }

                // Movement keys
                if (goup || godown || goright || goleft)
                {
                    // Mod the left two digits
                    if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_shift_down() && instrument_edit_digit_count == 2)
                    {
                        if (goup) (*edit) += 0x1000;
                        if (godown) (*edit) -= 0x1000;
                        if (goright) (*edit) += 0x0100;
                        if (goleft) (*edit) -= 0x0100;
                    }
                    // Mod the right two digits
                    else
                    {
                        if (goup && instrument_edit_digit_count != 0) (*edit) += 0x0010;
                        if (godown && instrument_edit_digit_count != 0)  (*edit) -= 0x0010;
                        if (goright) (*edit) += 0x0001;
                        if (goleft) (*edit) -= 0x0001;
                    }

                    // Correct number ranges
                    switch (instrument_edit_y)
                    {
                    case 3: // Channel Type
                        if ((*edit) < static_cast<int>(ChannelType::min))
                            (*edit) = static_cast<int>(ChannelType::max) + ((*edit) + 1);
                        if ((*edit) > static_cast<int>(ChannelType::max))
                            (*edit) = ((*edit) - 1) - static_cast<int>(ChannelType::max);
                        break;
                    case 4: // Waveform
                        if ((*edit) < static_cast<int>(SynthWaveForm::min))
                            (*edit) = static_cast<int>(SynthWaveForm::max) + ((*edit) + 1);
                        if ((*edit) > static_cast<int>(SynthWaveForm::max))
                            (*edit) = ((*edit) - 1) - static_cast<int>(SynthWaveForm::max);
                        break;
                    case 11: // Filter Type
                        if ((*edit) < static_cast<int>(FilterType::min))
                            (*edit) = static_cast<int>(FilterType::max) + ((*edit) + 1);
                        if ((*edit) > static_cast<int>(FilterType::max))
                            (*edit) = ((*edit) - 1) - static_cast<int>(FilterType::max);
                        break;
                    case 5: // Volume
                    case 6: // Panning
                    case 7: // Pulse Width
                    case 9: // Pitch Sweep
                    case 12: // Filter Cutoff
                    case 13: // Filter Resonance
                        // Wrap the cell between 0x0000 and 0xFFFF
                        if ((*edit) < 0)
                            (*edit) = 0xFFFF + ((*edit) + 1);
                        if ((*edit) > 0xFFFF)
                            (*edit) = ((*edit) - 1) - 0xFFFF;
                        break;
                    case 8: // Tuning
                        // Wrap the cell between 0x00 and 0xFF
                        if ((*edit) < 0)
                            (*edit) = 0xFF + ((*edit) + 1);
                        if ((*edit) > 0xFF)
                            (*edit) = ((*edit) - 1) - 0xFF;
                        break;
                    }

                    // Convert edits to actual values
                    instrumentlist[open_instrument]->detune = (instrumentlist[open_instrument]->detune_edit - 128) / 2.f;
                    std::string outstr;
                    outstr = IntToHexString(instrumentlist[open_instrument]->volume_edit);
                    outstr.insert(outstr.begin(), 4 - outstr.size(), '0');
                    instrumentlist[open_instrument]->volume = std::stoi(outstr.substr(0, 2), 0, 16) / 255.f;
                    instrumentlist[open_instrument]->volume_freq = (std::stoi(outstr.substr(2, 2), 0, 16) - 128) / 8.f;
                    outstr = IntToHexString(instrumentlist[open_instrument]->pan_edit);
                    outstr.insert(outstr.begin(), 4 - outstr.size(), '0');
                    instrumentlist[open_instrument]->panning = std::stoi(outstr.substr(0, 2), 0, 16) / 255.f;
                    instrumentlist[open_instrument]->pan_freq = std::stoi(outstr.substr(2, 2), 0, 16) / 16.f;
                    outstr = IntToHexString(instrumentlist[open_instrument]->pw_edit);
                    outstr.insert(outstr.begin(), 4 - outstr.size(), '0');
                    instrumentlist[open_instrument]->pulse_width = (std::stoi(outstr.substr(0, 2), 0, 16) / 255.f) - 0.5f;
                    instrumentlist[open_instrument]->pulse_width_freq = std::stoi(outstr.substr(2, 2), 0, 16) / 16.f;
                    instrumentlist[open_instrument]->pitch_freq = (instrumentlist[open_instrument]->pitch_freq_edit - 32768) / (65535.f / 2.f);
                    instrumentlist[open_instrument]->cutoff = instrumentlist[open_instrument]->cutoff_edit / 65535.f;
                    instrumentlist[open_instrument]->resonance = instrumentlist[open_instrument]->resonance_edit / 65535.f;
                }
            }
            // Goto page
            else if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_select_down())
            {
                // Go to the left page over
                if (goleft)
                {
                    // If the open_chain is valid
                    if (open_phrase != -1)
                    {
                        // Check if the phrase does not exist
                        if (GetAt(&phraselist, open_phrase) == nullptr)
                        {
                            InsertAt(&phraselist, open_phrase, new phrase());
                        }
                        // Chain
                        state = m_phrase;
                        cursor_x = SDL_clamp(cursor_x, 0, phrase_grid_w - 1);
                        cursor_y = SDL_clamp(cursor_y, 0, phrase_grid_h - 1);
                        breakend = true;
                    }
                }
                // Go to the above page over
                else if (goup)
                {
                    // TODO: Implement going to the wave editor
                }
            }
            // Moving
            else
            {
                cursor_x += goright - goleft;
                cursor_y += godown - goup;
            }

            // wrap the cursor and clamp offsets
            if (cursor_x > instrument::synth_menu_width - 1)
                cursor_x = 0;
            if (cursor_x < 0)
                cursor_x = instrument::synth_menu_width - 1;
            if (cursor_y > instrument::synth_menu_height - 1)
                cursor_y = 0;
            if (cursor_y < 0)
                cursor_y = instrument::synth_menu_height - 1;
        }
        // For editing file instrument
        else if (instrumentlist[open_instrument]->type == ChannelType::file)
        {
            // Handle deep copy input logic
            GetDeepCopyInputs();

            // If we have a successful deep copy, do it
            if (do_deep_copy == 2 && cursor_y == 1 && cursor_x == 0 && GetAt(&samplelist, instrumentlist[open_instrument]->sample_index) != nullptr)
            {
                // Deep copy the sample
                auto i = GetAt(&samplelist, instrumentlist[open_instrument]->sample_index)->DeepCopySample();
                auto index = GetNextEmpty(&samplelist);
                InsertAt(&samplelist, index, i);
                instrumentlist[open_instrument]->sample_index = index;
            }
            else
            {
                // Modify value
                if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_a_pressed())
                {
                    // Edit sample
                    if (cursor_y == 1 && cursor_x == 0)
                    {
                        // If the sample value is unfilled, insert 0
                        if (instrumentlist[open_instrument]->sample_index == -1)
                        {
                            if (copied_smple == -1)
                            {
                                // Set UI reference to Hex0
                                instrumentlist[open_instrument]->sample_index = 0x0000;
                                // If this sample doesn't exist yet, insert it
                                if (GetAt(&samplelist, instrumentlist[open_instrument]->sample_index) == nullptr)
                                    InsertAt(&samplelist, instrumentlist[open_instrument]->sample_index, new sample());
                            }
                            else
                            {
                                // Set UI reference to copied sample
                                instrumentlist[open_instrument]->sample_index = copied_smple;
                            }
                        }
                        // If double click, add a new sample
                        else if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->doubleclick)
                        {
                            // Set UI reference to the next empty
                            instrumentlist[open_instrument]->sample_index = GetNextEmpty(&samplelist);
                            // Add the new sample
                            InsertAt(&samplelist, instrumentlist[open_instrument]->sample_index, new sample());
                            // Copy to clipboard
                            copied_smple = instrumentlist[open_instrument]->sample_index;
                        }
                        else
                        {
                            // Copy to clipboard
                            copied_smple = instrumentlist[open_instrument]->sample_index;
                        }
                    }
                }

                // Toggle alt mode
                if ((instrument_edit_y == 12 || instrument_edit_y == 11 || instrument_edit_y == 10) && dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_select_pressed())
                    alt_mode = !alt_mode;

                // Editing
                if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_a_down())
                {
                    // Get edit ptr
                    int* edit;

                    // Default
                    edit = &(instrumentlist[open_instrument]->volume_edit);

                    // Get instrument edit pointer
                    switch (instrument_edit_y)
                    {
                    case 3: // Channel Type
                        edit = reinterpret_cast<int*>(&instrumentlist[open_instrument]->type);
                        break;
                    case 4: // Sample Index
                        edit = reinterpret_cast<int*>(&instrumentlist[open_instrument]->sample_index);
                        break;
                    case 5: // Volume
                        edit = &(instrumentlist[open_instrument]->volume_edit);
                        break;
                    case 6: // Panning
                        edit = &(instrumentlist[open_instrument]->pan_edit);
                        break;
                    case 7: // Tuning
                        edit = &(instrumentlist[open_instrument]->detune_edit);
                        break;
                    case 8: // Pitch Sweep
                        edit = &(instrumentlist[open_instrument]->pitch_freq_edit);
                        break;
                    case 9: // Base Pitch
                        edit = &(instrumentlist[open_instrument]->base_pitch);
                        break;
                    case 10: // Start Time
                        edit = &(instrumentlist[open_instrument]->start_time_ms);
                        break;
                    case 11: // Mid Time
                        edit = &(instrumentlist[open_instrument]->mid_time_ms);
                        break;
                    case 12: // End Time
                        edit = &(instrumentlist[open_instrument]->end_time_ms);
                        break;
                    case 13: // Loop
                        edit = &(instrumentlist[open_instrument]->loop);
                        break;
                    case 15: // Filter Type
                        edit = reinterpret_cast<int*>(&instrumentlist[open_instrument]->filter);
                        break;
                    case 16: // Filter Cutoff
                        edit = &(instrumentlist[open_instrument]->cutoff_edit);
                        break;
                    case 17: // Filter Resonance
                        edit = &(instrumentlist[open_instrument]->resonance_edit);
                        break;
                    }

                    // Movement keys
                    if (goup || godown || goright || goleft)
                    {
                        // Mod the left two digits
                        if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_shift_down() && instrument_edit_digit_count == 3)
                        {
                            if (alt_mode)
                            {
                                if (goup) (*edit) += 0x100000;
                                if (godown) (*edit) -= 0x100000;
                                if (goright) (*edit) += 0x010000;
                                if (goleft) (*edit) -= 0x010000;
                            }
                            else
                            {
                                if (goup) (*edit) += 0x001000;
                                if (godown) (*edit) -= 0x001000;
                                if (goright) (*edit) += 0x000100;
                                if (goleft) (*edit) -= 0x000100;
                            }
                        }
                        else if (instrument_edit_digit_count == 4)
                        {
                            (*edit) = (*edit) == 0 ? 1 : 0;
                        }
                        else if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_shift_down() && instrument_edit_digit_count == 2)
                        {
                            if (goup) (*edit) += 0x1000;
                            if (godown) (*edit) -= 0x1000;
                            if (goright) (*edit) += 0x0100;
                            if (goleft) (*edit) -= 0x0100;
                        }
                        // Mod a note
                        else if (instrument_edit_digit_count == 1.5)
                        {
                            // Edit note pitch
                            if (goup) (*edit) += 12;
                            if (godown) (*edit) -= 12;
                            if (goright) (*edit) += 1;
                            if (goleft) (*edit) -= 1;
                        }
                        // Mod the right two digits
                        else if (instrument_edit_digit_count == 3)
                        {
                            if (alt_mode)
                            {
                                if (goup) (*edit) += 0x001000;
                                if (godown) (*edit) -= 0x001000;
                                if (goright) (*edit) += 0x000100;
                                if (goleft) (*edit) -= 0x000100;
                            }
                            else
                            {
                                if (goup) (*edit) += 0x000010;
                                if (godown) (*edit) -= 0x000010;
                                if (goright) (*edit) += 0x000001;
                                if (goleft) (*edit) -= 0x000001;
                            }
                        }
                        else
                        {
                            if (goup && instrument_edit_digit_count != 0) (*edit) += 0x0010;
                            if (godown && instrument_edit_digit_count != 0)  (*edit) -= 0x0010;
                            if (goright) (*edit) += 0x0001;
                            if (goleft) (*edit) -= 0x0001;
                        }

                        // Correct number ranges
                        switch (instrument_edit_y)
                        {
                        case 3: // Channel Type
                            if ((*edit) < static_cast<int>(ChannelType::min))
                                (*edit) = static_cast<int>(ChannelType::max) + ((*edit) + 1);
                            if ((*edit) > static_cast<int>(ChannelType::max))
                                (*edit) = ((*edit) - 1) - static_cast<int>(ChannelType::max);
                            break;
                        case 15: // Filter Type
                            if ((*edit) < static_cast<int>(FilterType::min))
                                (*edit) = static_cast<int>(FilterType::max) + ((*edit) + 1);
                            if ((*edit) > static_cast<int>(FilterType::max))
                                (*edit) = ((*edit) - 1) - static_cast<int>(FilterType::max);
                            break;
                        case 9: // Base pitch
                            if ((*edit) < min_note)
                                (*edit) = max_note + (((*edit) - min_note) + 1);
                            if ((*edit) > max_note)
                                (*edit) = min_note + ((*edit) - 1) - max_note;
                            break;
                        case 10: // Start Time
                        case 11: // Mid Time
                        case 12: // End Time
                            // Wrap the cell between 0x000000 and 0xFFFFFF
                            if ((*edit) < 0)
                                (*edit) = 0xFFFFFF + ((*edit) + 1);
                            if ((*edit) > 0xFFFFFF)
                                (*edit) = ((*edit) - 1) - 0xFFFFFF;
                            break;
                        case 4: // Sample index
                        case 5: // Volume
                        case 6: // Panning
                        case 8: // Pitch Sweep
                        case 16: // Filter Cutoff
                        case 17: // Filter Resonance
                            // Wrap the cell between 0x0000 and 0xFFFF
                            if ((*edit) < 0)
                                (*edit) = 0xFFFF + ((*edit) + 1);
                            if ((*edit) > 0xFFFF)
                                (*edit) = ((*edit) - 1) - 0xFFFF;
                            break;
                        case 7: // Tuning
                            // Wrap the cell between 0x00 and 0xFF
                            if ((*edit) < 0)
                                (*edit) = 0xFF + ((*edit) + 1);
                            if ((*edit) > 0xFF)
                                (*edit) = ((*edit) - 1) - 0xFF;
                            break;
                        }

                        // Convert edits to actual values
                        instrumentlist[open_instrument]->detune = (instrumentlist[open_instrument]->detune_edit - 128) / 2.f;
                        std::string outstr;
                        outstr = IntToHexString(instrumentlist[open_instrument]->volume_edit);
                        outstr.insert(outstr.begin(), 4 - outstr.size(), '0');
                        instrumentlist[open_instrument]->volume = std::stoi(outstr.substr(0, 2), 0, 16) / 255.f;
                        instrumentlist[open_instrument]->volume_freq = (std::stoi(outstr.substr(2, 2), 0, 16) - 128) / 8.f;
                        outstr = IntToHexString(instrumentlist[open_instrument]->pan_edit);
                        outstr.insert(outstr.begin(), 4 - outstr.size(), '0');
                        instrumentlist[open_instrument]->panning = std::stoi(outstr.substr(0, 2), 0, 16) / 255.f;
                        instrumentlist[open_instrument]->pan_freq = std::stoi(outstr.substr(2, 2), 0, 16) / 16.f;
                        instrumentlist[open_instrument]->pitch_freq = (instrumentlist[open_instrument]->pitch_freq_edit - 32768) / (65535.f / 2.f);
                        instrumentlist[open_instrument]->cutoff = instrumentlist[open_instrument]->cutoff_edit / 65535.f;
                        instrumentlist[open_instrument]->resonance = instrumentlist[open_instrument]->resonance_edit / 65535.f;
                    }
                }
                // Goto page
                else if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_select_down())
                {
                    // Go to the left page over
                    if (goleft)
                    {
                        // Reset alt mode
                        alt_mode = false;
                        // If the open_chain is valid
                        if (open_phrase != -1)
                        {
                            // Check if the phrase does not exist
                            if (GetAt(&phraselist, open_phrase) == nullptr)
                            {
                                InsertAt(&phraselist, open_phrase, new phrase());
                            }
                            // Chain
                            state = m_phrase;
                            cursor_x = SDL_clamp(cursor_x, 0, phrase_grid_w - 1);
                            cursor_y = SDL_clamp(cursor_y, 0, phrase_grid_h - 1);
                            breakend = true;
                        }
                    }
                    // Go to the above page over
                    else if (goup)
                    {
                        // Reset alt mode
                        alt_mode = false;
                        // Set open sample
                        if (cursor_y == 1 && cursor_x == 0 && instrumentlist[open_instrument]->sample_index != -1)
                        {
                            open_sample = instrumentlist[open_instrument]->sample_index;
                        }
                        // If the open_sample is valid
                        if (open_sample != -1)
                        {
                            // Check if the sample does not exist
                            if (GetAt(&samplelist, open_sample) == nullptr)
                            {
                                InsertAt(&samplelist, open_sample, new sample());
                            }
                            // Sample
                            state = m_wave;
                            sample_offset_y = 0;
                            cursor_x = 0;
                            cursor_y = 0;
                            breakend = true;
                        }
                    }
                }
                // Moving
                else
                {
                    // Reset alt mode
                    if (goright || goleft || godown || goup)
                        alt_mode = false;
                    cursor_x += goright - goleft;
                    cursor_y += godown - goup;
                }

                // wrap the cursor and clamp offsets
                if (cursor_x > instrument::sample_menu_width - 1)
                    cursor_x = 0;
                if (cursor_x < 0)
                    cursor_x = instrument::sample_menu_width - 1;
                if (cursor_y > instrument::sample_menu_height - 1)
                    cursor_y = 0;
                if (cursor_y < 0)
                    cursor_y = instrument::sample_menu_height - 1;
            }

            // Reset deep copy action flag
            if (do_deep_copy == 2)
                do_deep_copy = 0;
        }

        // Update UI
        DrawInstrumentUI();
    }

    // Handle input for the wave menu
    if (state == m_wave && !breakend)
    {
        if (instrumentlist[open_instrument]->type == ChannelType::file) // Sample loader
        {
            // TODO: Implement safety for missing files or directories (Like if the user deletes the directory or file)

            // Modify value or open dir
            if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_a_pressed())
            {
                // Open directory
                if (selected_path_isdir)
                {
                    current_dir_length = 0;
                    sample_offset_y = 0;
                    cursor_y = 0;
                    current_dir = selected_path;
                }
                // Open file
                else
                {
                    samplelist[open_sample]->path = selected_path;
                    int at = 0;
                    for (auto c : samplelist[open_sample]->path)
                        samplelist[open_sample]->path[at++] = (char)toupper(c);

                    // Load sample audio into memory and assign index to sample object
                    if (samplelist[open_sample]->sound_index != -1 && geptr->CheckSound(samplelist[open_sample]->sound_index) == true)
                        geptr->DeleteSound(samplelist[open_sample]->sound_index);
                    samplelist[open_sample]->sound_index = geptr->AddSound(samplelist[open_sample]->path.c_str());

                    // Preview the audio
                    geptr->SetChannelPitchRatio(open_channel, 1);
                    geptr->SetChannelVolume(open_channel, 1);
                    geptr->SetChannelPanning(open_channel, 0.5);
                    geptr->PlaySoundOnChannel(samplelist[open_sample]->sound_index, open_channel, false);
                }
            }
            // Go up a directory
            if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_b_pressed())
            {
                if (current_dir != main_dir)
                {
                    // Remove trailing backslash
                    current_dir = current_dir.substr(0, current_dir.length() - 1);
                    // Remove all to the last backslash
                    current_dir = current_dir.substr(0, current_dir.find_last_of('\\')+1);
                    // Reset
                    current_dir_length = 0;
                    sample_offset_y = 0;
                    cursor_y = 0;
                }
            }
            // Goto page
            else if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_select_down())
            {
                // Go to the down page over
                if (godown)
                {
                    // If the open_instrument is valid
                    if (open_instrument != -1)
                    {
                        // Check if the instrument does not exist
                        if (GetAt(&instrumentlist, open_instrument) == nullptr)
                        {
                            InsertAt(&instrumentlist, open_instrument, new instrument());
                        }
                        // Chain
                        state = m_instrument;
                        cursor_x = SDL_clamp(cursor_x, 0, GetAt(&instrumentlist, open_instrument)->type == ChannelType::synth ? instrument::synth_menu_width - 1 : instrument::sample_menu_width - 1);
                        cursor_y = SDL_clamp(cursor_y, 0, GetAt(&instrumentlist, open_instrument)->type == ChannelType::synth ? instrument::synth_menu_height - 1 : instrument::sample_menu_height - 1);
                        breakend = true;
                    }
                }
            }
            // Moving
            else
            {
                cursor_y += godown - goup;
            }

            // wrap the cursor and clamp offsets
            if (cursor_y + sample_offset_y > current_dir_length - 1)
            {
                cursor_y = 0;
                sample_offset_y = 0;
            }
            if (cursor_y + sample_offset_y < 0)
            {
                cursor_y = file_display_count - 1;
                sample_offset_y = std::max(0, current_dir_length - file_display_count);
            }

            // Move the page
            if (cursor_y > file_display_count - 1)
                sample_offset_y += 1;
            if (cursor_y < 0)
                sample_offset_y -= 1;

            cursor_y = SDL_clamp(cursor_y, 0, file_display_count - 1);
            sample_offset_y = SDL_clamp(sample_offset_y, 0, std::max(0, current_dir_length - file_display_count));
        }
        else // Waveform editor
        {

        }

        // Stop previewing the audio
        if (dynamic_cast<input*>(geptr->GetObjectReference(inputgetter))->is_a_released())
        {
            geptr->StopChannel(open_channel);
        }

        // Update UI
        DrawWaveUI();
    }

    // Draw the map for where you are in the UI
    DrawMap();

    // Handle repeating movement from hold presses
    HandleMovementRepeaters();

    // If we want to pause then pause
    if (willpause)
    {
        // Stop audio
        StopAllChannels();
        // Pause song
        pause_song = willpause;
    }
}

// Master pre code
void GameInit()
{
    // Set UI grid w and h
    song_grid_h = std::min(rowcount, geptr->GetCanvasH() - 4);
    song_grid_w = std::min(channelcount, (geptr->GetCanvasW() - 8) / 5);
    phrase_grid_h = std::min(16, geptr->GetCanvasH() - 4);
    phrase_grid_w = 8;
    chain_grid_h = std::min(16, geptr->GetCanvasH() - 4);
    chain_grid_w = 2;
    file_display_count = song_grid_h - 6;

    // Init channel sequencers
    for (int i = 0; i < channelcount; i++)
    {
        channellist[i].channelnumber = i;
        channellisttypeptr[i] = &(channellist[i].type);
        sampleautomatorlist[i].channelnumber = i;
        synthlist[i] = new GravityEngine_Synth();
    }

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
    DrawSongUI(0, 0);

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

    // Debug : Draw channel 0's sequence
    geptr->DrawTextString(10, 0, geptr->entity,
        std::to_string(channellist[0].chain_ptr) + " - " +
        std::to_string(channellist[0].phrase_ptr) + " - " +
        std::to_string(channellist[0].step_ptr) + " - " +
        std::to_string(channellist[0].tick_ptr) + " " + 
        std::to_string(do_deep_copy) + "   ",
        primary_text_a);
}

// Master post code
void PostGameLoop()
{
}

// Master exit game code
void ExitGameLoop()
{
    // Stop playing
    running = false;
    while (play_thread) {};
}

int main()
{
    // Init engine - 128x72 is generally the largest you can get and still maintain good performance
    auto cw = 96 / 2 + 1;
    auto ch = 54 / 2;
    GravityEngine_Core ge_inst = GravityEngine_Core("Game", "com.example.game", "1.0", std::max(cw, 37), std::max(ch, 28), fps, 1920, 1080, "./GameFont.ttf", channelcount);

    ge_inst.debug_mode = true; // Show debug overlay
    ge_inst.debug_complex = false; // Show all information
    geptr = &ge_inst; // Set the pointer to the console engine class

    // Start game loop
    ge_inst.Start(&GameInit, &PreGameLoop, &PostGameLoop, &ExitGameLoop);

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
