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

// Enum to define the type of filter applied to audio channel
enum class FilterType
{
    lowpass,
    highpass,
    bandpass,
    none,
    min = lowpass,
    max = none
};

// Enum to define filter algorithm
enum class FilterAlgorithm
{
    chamberlain
};