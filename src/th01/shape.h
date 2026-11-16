#pragma once

#include "th01/video.h"

// Draws polar dots, flushing each cached byte only when the next byte
// differs. Retains the missing final flush and current-X guard on the
// previous byte. Uses linear planar addresses within the visible page.
// Equal start/end draws nothing; reversed ranges and zero step do nothing.
// Allocates nothing and preserves page selectors.
void Shape_EllipseArcPut(
    VIDEO *video, int16_t center_x, int16_t center_y, int16_t radius_x,
    int16_t radius_y, uint8_t color, uint8_t step, uint8_t start, uint8_t end);

// Restores page 1 to page 0 in aligned word rectangles along the arc.
// Restores the first sample unconditionally, defining the source's
// uninitialized previous coordinates. This presentation safety extension
// has no claim of parity with the source's first stack-dependent restore.
void Shape_EllipseArcErase(
    VIDEO *video, int16_t center_x, int16_t center_y, int16_t radius_x,
    int16_t radius_y, uint8_t step, uint8_t start, uint8_t end);
