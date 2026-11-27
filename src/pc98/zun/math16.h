#pragma once

#include "core/base/types.h"

typedef struct {
    int16_t x;
    int16_t y;
} MATH16_VECTOR;

// Sine and cosine scaled by 256, 256 units per turn, positive sine pointing
// down the screen.
int16_t Math16_Sin(int32_t angle);
int16_t Math16_Cos(int32_t angle);

// Wraps to the signed low word.
int16_t Math16_Wrap(int32_t value);

// Adds the length times the cosine or sine, rounded down, to the center.
int16_t Math16_PolarX(int16_t center, int16_t length, int32_t angle);
int16_t Math16_PolarY(int16_t center, int16_t length, int32_t angle);

// Returns the amplitude times the sine, divided toward zero.
int16_t Math16_Wave(int16_t amplitude, int32_t angle);

// Returns the byte angle of a direction, zero for the zero vector. An
// INT16_MIN component uses its unsigned magnitude, where the original's
// signed comparison picks an overflowing division.
uint8_t Math16_Angle(int16_t y, int16_t x);
MATH16_VECTOR Math16_Vector(int16_t length, int32_t angle);

// Aims from one point at another, adding the offset to the angle. The
// differences wrap to words, as the original's 16-bit subtraction does.
MATH16_VECTOR Math16_Aim(
    MATH16_VECTOR from, MATH16_VECTOR to, int16_t length, uint8_t offset);

// Rounds the square root to the nearest integer. Negative inputs and
// inputs above 0x3fff8000 return zero, as in master.lib.
int16_t Math16_Sqrt(int32_t value);
int16_t Math16_Hypot(int16_t x, int16_t y);

// Converts between pixels and Q12.4, rounding down toward pixels.
int16_t Math16_FromPixel(int16_t pixel);
int16_t Math16_ToPixel(int16_t subpixel);
