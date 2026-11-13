// SPDX-License-Identifier: GPL-3.0-only

#include "core/math/x87.h"

#include "core/math/libm/libm.h"

#include <math.h>

float X87_Sin(const float x)
{
    return (float)otm_Sin((double)x);
}

float X87_Cos(const float x)
{
    return (float)otm_Cos((double)x);
}

float X87_Tan(const float x)
{
    return (float)otm_Tan((double)x);
}

float X87_Atan2(const float y, const float x)
{
    return (float)otm_Atan2((double)y, (double)x);
}

void X87_SinCos(const float angle, float *const sin, float *const cos)
{
    const double a = (double)angle;
    *sin = (float)otm_Sin(a);
    *cos = (float)otm_Cos(a);
}

void X87_SinCosScale(
    const float angle, const float radius, float *const x, float *const y)
{
    const double a = (double)angle;
    const double r = (double)radius;
    *x = (float)(otm_Cos(a) * r);
    *y = (float)(otm_Sin(a) * r);
}

float X87_Rint(const float x)
{
    if (!isfinite(x) || fabsf(x) >= 8388608.0f) { // 2**23: already integral
        return x;
    }
    const float f = floorf(x);
    const float diff = x - f;
    float result;
    if (diff < 0.5f) {
        result = f;
    } else if (diff > 0.5f) {
        result = f + 1.0f;
    } else { // an exact tie rounds to the even integer
        result = fmodf(f, 2.0f) == 0.0f ? f : f + 1.0f;
    }
    return copysignf(result, x);
}

int32_t X87_Ftol(const float x)
{
    if (!isfinite(x)) {
        return INT32_MIN;
    }
    const double truncated = trunc((double)x);
    if (truncated < -2147483648.0 || truncated >= 2147483648.0) {
        return INT32_MIN;
    }
    return (int32_t)truncated;
}

float X87_Sqrt(const float x)
{
    return sqrtf(x);
}
