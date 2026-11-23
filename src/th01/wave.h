#pragma once

#include "th01/video.h"

// Restores one aligned row from page 1 to page 0 at each sine-displaced X
// coordinate. Wraps coordinates and phase to signed 16 bits. Divides the
// amplitude product toward zero; wavelengths above 256 keep a constant
// phase. Requires a nonzero wavelength and a positive width. Selects drawing
// page 0, except that a nonpositive height draws nothing.
void Wave_Restore(
    VIDEO *video, int16_t left, int16_t top, int16_t wavelength,
    int16_t amplitude, int16_t phase, int16_t width, int16_t height);
