// SPDX-License-Identifier: GPL-3.0-only

#include "core/math/common.h"

#include <math.h>

float Math_Abs(const float x)
{
    return fabsf(x);
}

float Math_Min(const float a, const float b)
{
    return a <= b ? a : b;
}

float Math_Max(const float a, const float b)
{
    return a >= b ? a : b;
}

float Math_Clamp(const float x, const float min, const float max)
{
    return Math_Max(min, Math_Min(max, x));
}

float Math_Lerp(const float a, const float b, const float t)
{
    return a + (b - a) * t;
}

float Math_Fmod(const float x, const float y)
{
    return fmodf(x, y);
}
