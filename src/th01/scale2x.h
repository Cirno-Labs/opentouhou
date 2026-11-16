#pragma once

#include "th01/video.h"

// Doubles a visible byte-aligned rectangle from page 1 onto page 0. Rounds
// both left coordinates down to eight pixels and width down to 16. Index 0
// is transparent; each other source pixel covers a 2x2 destination square.
// Requires both rounded rectangles entirely inside their pages. Nonpositive
// height or width below 16 draws nothing and preserves the drawing selector;
// other calls select page 0. Preserves page 1, display selection and palette.
// Uses caller-owned pages, allocates nothing and performs no IO.
void Scale2x_Copy(
    VIDEO *video, int16_t destination_left, int16_t destination_top,
    int16_t source_left, int16_t source_top, int16_t width, int16_t height);
