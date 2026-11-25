#pragma once

#include "th01/video.h"

// Restores each row from page 1 to page 0 at a sine-displaced X.
// Requires a nonzero wavelength and a positive width.
void Wave_Restore(
    int16_t left, int16_t top, int16_t wavelength, int16_t amplitude,
    int16_t phase, int16_t width, int16_t height);
