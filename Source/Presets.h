#pragma once
#include <JuceHeader.h>

namespace MoonVerbPresets
{
    struct Preset
    {
        const char* name;
        float mix, decay, damping, width, shimmer;
        float preDelayMs, modRate, modDepth;
        bool freeze;
    };

    static constexpr int count = 15;

    static const Preset presets[count] = {
        { "Default",          0.30f, 0.50f, 0.30f, 1.0f, 0.00f,  20.0f, 0.50f, 0.20f, false },
        { "Lush Hall",        0.40f, 0.75f, 0.25f, 1.0f, 0.00f,  30.0f, 0.30f, 0.30f, false },
        { "Small Room",       0.35f, 0.25f, 0.40f, 0.6f, 0.00f,   5.0f, 0.50f, 0.10f, false },
        { "Dark Cathedral",   0.45f, 0.85f, 0.70f, 1.0f, 0.00f,  60.0f, 0.20f, 0.20f, false },
        { "Bright Plate",     0.35f, 0.55f, 0.10f, 0.8f, 0.00f,  10.0f, 0.80f, 0.15f, false },
        { "Shimmer Pad",      0.50f, 0.80f, 0.20f, 1.0f, 0.60f,  40.0f, 0.40f, 0.30f, false },
        { "Frozen Landscape", 0.60f, 0.90f, 0.30f, 1.0f, 0.15f,  50.0f, 0.10f, 0.10f, true  },
        { "Ethereal Wash",    0.70f, 0.95f, 0.15f, 1.0f, 1.00f,  80.0f, 0.30f, 0.40f, false },
        { "Tape Room",        0.30f, 0.45f, 0.50f, 0.7f, 0.00f,  80.0f, 0.60f, 0.20f, false },
        { "Modulated Space",  0.40f, 0.65f, 0.30f, 1.0f, 0.10f,  25.0f, 3.00f, 0.60f, false },
        { "Tight Ambience",   0.20f, 0.15f, 0.35f, 0.5f, 0.00f,   3.0f, 0.50f, 0.10f, false },
        { "Dreamy Clouds",    0.55f, 0.80f, 0.20f, 1.0f, 0.40f,  45.0f, 0.15f, 0.35f, false },
        { "Infinite Void",    0.65f, 1.00f, 0.25f, 1.0f, 0.30f, 100.0f, 0.20f, 0.30f, false },
        { "Vocal Plate",      0.25f, 0.50f, 0.20f, 0.8f, 0.00f,  30.0f, 0.40f, 0.15f, false },
        { "Lo-Fi Reverb",     0.35f, 0.50f, 0.90f, 0.5f, 0.00f,  15.0f, 0.30f, 0.10f, false },
    };
}
