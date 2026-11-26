#pragma once

// The presenter shows a frame: the graphics page through the 16-color
// palette, with text RAM on top in its fixed digital colors.

#include "th02/tram.h"
#include "th02/video.h"

RESULT Presenter_Init(void);
void Presenter_Close(void);

// Draws the frame's graphics, then its text in fixed digital colors.
void Presenter_Draw(
    bool text_visible, const GAIJI *gaiji, const TRAM_FONT *font,
    uint16_t first_cell, bool blink_visible);
