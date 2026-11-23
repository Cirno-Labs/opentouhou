#pragma once

#include "th01/video.h"

// Draws polar dots, flushing each cached byte only when the next byte
// differs. Keeps the original's missing final flush and its current-X guard
// on the previous byte. Uses linear planar addresses within the visible
// page. Draws nothing when start is at or past end or step is zero.
void Shape_EllipseArcPut(
    int16_t center_x, int16_t center_y, int16_t radius_x, int16_t radius_y,
    uint8_t color, uint8_t step, uint8_t start, uint8_t end);

// Restores page 1 to page 0 in aligned word rectangles along the arc. Always
// restores the first sample, where the original reads uninitialized
// previous coordinates.
void Shape_EllipseArcErase(
    int16_t center_x, int16_t center_y, int16_t radius_x, int16_t radius_y,
    uint8_t step, uint8_t start, uint8_t end);
