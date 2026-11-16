#pragma once

#include "core/base/types.h"

typedef struct {
    int16_t x, y;
} MATH_VECTOR;

// Advances the 32-bit master.lib random state and returns 0 through 32767.
// The initial state is 1. Separate simulations keep separate states.
uint16_t Math_Random(uint32_t *state);

// Returns the stock integer sine or cosine scaled by 256. Angles wrap
// every 256 units, with positive sine pointing down the screen.
int16_t Math_Sin8(int32_t angle);
int16_t Math_Cos8(int32_t angle);

// Wraps an integer to signed 16 bits without signed overflow.
int16_t Math_Wrap16(int32_t value);

// Returns a polar coordinate with the product rounded down before adding
// the center. The final coordinate wraps to signed 16 bits.
int16_t Math_PolarX(int16_t center, int16_t radius, int32_t angle);
int16_t Math_PolarY(int16_t center, int16_t radius, int32_t angle);

// Returns a sine displacement with the product divided toward zero.
int16_t Math_WaveX(int16_t amplitude, int32_t angle);

// Returns the master.lib lookup angle from a signed direction, or zero
// for the zero vector. For INT16_MIN, uses an unsigned magnitude; the
// original's signed comparison can select an overflowing division there.
uint8_t Math_Angle(int16_t y, int16_t x);
MATH_VECTOR Math_Vector(int16_t length, int32_t angle);

// Wraps each coordinate difference to signed 16 bits before finding its
// angle, as the original 16-bit subtraction does.
MATH_VECTOR Math_VectorBetween(
    MATH_VECTOR from, MATH_VECTOR to, int16_t length);

// Returns the nearest integer square root. Returns zero for negative
// inputs and inputs greater than 0x3fff8000, matching master.lib.
int16_t Math_Sqrt(int32_t value);

// Applies the same root and overflow limit to x*x + y*y.
int16_t Math_Hypot(int16_t x, int16_t y);

// Converts integer pixels to Q12.4 with 16-bit wrap. Converts Q12.4 back
// to integer pixels by rounding down, including negative fractions.
int16_t Math_SubpixelFromPixel(int16_t pixel);
int16_t Math_SubpixelToPixel(int16_t subpixel);
