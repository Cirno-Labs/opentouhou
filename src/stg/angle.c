#include "stg/angle.h"

#include "core/math/common.h"
#include "core/math/x87.h"

#define M_TURN_LIMIT 16

float Angle_AddNormalize(float a, const float b)
{
    int32_t turns = 0;
    a += b;
    while (a > MATH_PI) {
        a -= MATH_TWO_PI;
        if (turns++ > M_TURN_LIMIT) {
            break;
        }
    }
    while (a < -MATH_PI) {
        a += MATH_TWO_PI;
        if (turns++ > M_TURN_LIMIT) {
            break;
        }
    }
    return a;
}

void Angle_Rotate2D(VEC3 *const out, const VEC3 *const in, const float angle)
{
    const float s = X87_Sin(angle);
    const float c = X87_Cos(angle);
    out->x = c * in->x + s * in->y;
    out->y = c * in->y - s * in->x;
}
