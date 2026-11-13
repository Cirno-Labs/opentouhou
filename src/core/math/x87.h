// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "core/base/types.h"

// Reproduces what the original games got from the x87 FPU, running with
// its precision control set to 24 bits (the mode Direct3D 8 selects for a
// device created without D3DCREATE_FPU_PRESERVE; see docs/ARCHITECTURE.md,
// "Floating point", and each game's own notes). Simulation code must call
// these, never the platform's trig functions or type-conversion rules,
// because glibc, MinGW's msvcrt and Emscripten's musl do not all agree
// with the x87 bit for bit.
//
// The trig functions compute in double precision from a vendored libm
// (see core/math/libm/libm_internal.h) and round the double result to
// float once, which is what the x87 does: it keeps intermediate results
// at full (80-bit extended, here widened to double) precision and only
// the fstp that stores the result rounds to the destination's precision.
// tests/test_x87.c checks them against the real instructions on x86.

// Returns sin(x), cos(x) and tan(x), reproducing fsin, fcos and fptan at
// 24-bit precision.
float X87_Sin(float x);
float X87_Cos(float x);
float X87_Tan(float x);

// Returns atan2(y, x), reproducing fpatan at 24-bit precision.
float X87_Atan2(float y, float x);

// Returns sin(x) and cos(x) together, reproducing fsincos at 24-bit
// precision. fsincos computes both from one argument reduction, but (since
// it does not round until the two fstp that store its results) gives the
// same two values X87_Sin and X87_Cos give separately.
void X87_SinCos(float angle, float *sin, float *cos);

// Sets *x = cos(angle) * radius and *y = sin(angle) * radius, reproducing
// fsincos followed by two fmuls at 24-bit precision. fsincos leaves its
// results at extended precision, so each product is rounded to float only
// once, after the multiply: *x and *y are not simply X87_Cos(angle) *
// radius and X87_Sin(angle) * radius, which round the trig result to
// float first and then round the product again (double rounding can give
// a different last bit).
void X87_SinCosScale(float angle, float radius, float *x, float *y);

// Returns x rounded to the nearest integer, reproducing frndint in the
// default round-to-nearest-even mode. Ties round to the even integer:
// X87_Rint(0.5) is 0, X87_Rint(1.5) is 2, X87_Rint(2.5) is 2.
float X87_Rint(float x);

// Truncates x toward zero and returns it as a 32-bit integer, reproducing
// _ftol (fistp after setting the rounding mode to truncate). NaN and a
// value outside [INT32_MIN, INT32_MAX] return INT32_MIN, the x87 "integer
// indefinite", instead of invoking C's undefined behaviour for an
// out-of-range conversion.
int32_t X87_Ftol(float x);

// Returns the square root of x. The x87's fsqrt and SSE2's sqrtf are both
// correctly rounded (IEEE 754 requires it for all basic operations
// including sqrt), so this needs no double-rounding or vendored-libm
// treatment; it exists here so simulation code has one place to get every
// x87 primitive from.
float X87_Sqrt(float x);
