#pragma once

// Float helpers that are exact by nature: every compiler and every FPU
// mode gives the same bits, so there is nothing "replay-exact" to
// reproduce and no "fast" variant to pick from. Unlike core/math/x87, these
// are safe to use outside the simulation too.

// Returns the absolute value of x. Flips the sign bit; NaN keeps its
// payload, with the sign bit cleared.
float Math_Abs(float x);

// Return the smaller and the larger of a and b, using a plain comparison
// (a <= b for Min, a >= b for Max). A NaN argument makes the comparison
// false, so Math_Min(a, NaN) and Math_Max(a, NaN) both return the NaN (b),
// and Math_Min(NaN, b) and Math_Max(NaN, b) both return b.
float Math_Min(float a, float b);
float Math_Max(float a, float b);

// Clamps x to [min, max]. The result is unspecified if min > max.
float Math_Clamp(float x, float min, float max);

// Interpolates linearly between a and b: a + (b - a) * t. t = 0 gives a, t
// = 1 gives b exactly (not an approximation), because (b - a) * 1 added
// back to a is a and b are rounded the same way they would be by direct
// subtraction and addition. This is the formula and operand order D3DX8
// and the originals use; a different formula or order rounds differently.
float Math_Lerp(float a, float b, float t);

// Returns the floating-point remainder of x / y, with the sign of x, same
// as C's fmod. This is what the x87 fprem instruction computes (truncated
// division, unlike fprem1's round-to-nearest), so it needs no
// replay-exact wrapper of its own.
float Math_Fmod(float x, float y);
