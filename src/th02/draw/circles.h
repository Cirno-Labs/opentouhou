#pragma once

// Dots and circles in one color through the charger, clipped the way the
// original clips them.

#include "core/base/types.h"

// Draws a square dot of size 0..5, ignoring clipping and row-400 wrap.
void Draw_Dot(int16_t left, int16_t top, uint8_t size);

// Draws a circle; as in retail, lower points on the bottom row drop out
// when it crosses a clip edge.
void Draw_Circle(int16_t x, int16_t y, uint16_t radius);

// Fills a circle; radius zero writes one clipped byte mask.
void Draw_CircleFill(int16_t x, int16_t y, uint16_t radius);
