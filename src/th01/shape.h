#pragma once

#include "th01/video.h"

// Draws the dots of an ellipse arc from start to end in steps of step.
// Keeps the original's missing final byte flush.
void Shape_EllipseArcPut(
    int16_t center_x, int16_t center_y, int16_t radius_x, int16_t radius_y,
    uint8_t color, uint8_t step, uint8_t start, uint8_t end);

// Restores page 1 to page 0 in aligned word rectangles along the arc.
void Shape_EllipseArcErase(
    int16_t center_x, int16_t center_y, int16_t radius_x, int16_t radius_y,
    uint8_t step, uint8_t start, uint8_t end);
