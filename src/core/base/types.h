// SPDX-License-Identifier: GPL-3.0-only

#pragma once

// Provides the fixed-width integer types and checks the properties of the
// basic types that the code depends on. The simulations reproduce the
// original games' float results, which requires IEEE 754 single and double
// precision.

#include <float.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>

static_assert(CHAR_BIT == 8);
static_assert(sizeof(float) == 4 && FLT_RADIX == 2 && FLT_MANT_DIG == 24);
static_assert(sizeof(double) == 8 && DBL_MANT_DIG == 53);
