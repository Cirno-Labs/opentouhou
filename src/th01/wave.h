#pragma once

#include "th01/video.h"

// Restores one aligned EGC row at each sine-displaced X coordinate, using
// page 1 as the source and page 0 as the destination. Wraps coordinates and
// phase to signed 16 bits after each source operation. Divides the amplitude
// product toward zero; wavelengths above 256 keep a constant phase.
// Requires nonzero wavelength and positive width. Nonpositive height draws
// nothing and preserves the selector; other calls finish on page 0.
// Uses caller-owned pages, allocates nothing and performs no IO.
void Wave_Restore(
    VIDEO *video, int16_t left, int16_t top, int16_t wavelength,
    int16_t amplitude, int16_t phase, int16_t width, int16_t height);
