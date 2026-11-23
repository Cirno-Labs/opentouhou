#pragma once

#include "th01/video.h"

// Doubles a byte-aligned rectangle from page 1 onto page 0: each nonzero
// source pixel covers a 2x2 square, and index 0 is transparent. Rounds both
// left coordinates down to eight pixels and width down to 16. Requires both
// rounded rectangles inside their pages. Selects drawing page 0, except
// that a nonpositive height or a width below 16 draws nothing and keeps the
// selected page.
void Scale2x_Copy(
    VIDEO *video, int16_t destination_left, int16_t destination_top,
    int16_t source_left, int16_t source_top, int16_t width, int16_t height);
