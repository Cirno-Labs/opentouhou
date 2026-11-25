#pragma once

#include "th01/video.h"

// Doubles a rectangle from page 1 onto page 0, with index 0 transparent.
// Rounds lefts down to 8 and width down to 16; both must fit their pages.
void Scale2x_Copy(
    int16_t destination_left, int16_t destination_top, int16_t source_left,
    int16_t source_top, int16_t width, int16_t height);
