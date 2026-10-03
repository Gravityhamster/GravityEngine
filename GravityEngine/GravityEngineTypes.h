#pragma once

// Enum to define the current playback state of a sound channel
enum ChannelStates
{
    stopped,
    init,
    playing,
    paused,
    uninit
};