#include "pc98/zun/math16.h"

static const int16_t m_Sine[65] = {
    0,   6,   13,  19,  25,  31,  38,  44,  50,  56,  62,  68,  74,
    80,  86,  92,  98,  104, 109, 115, 121, 126, 132, 137, 142, 147,
    152, 157, 162, 167, 172, 177, 181, 185, 190, 194, 198, 202, 206,
    209, 213, 216, 220, 223, 226, 229, 231, 234, 237, 239, 241, 243,
    245, 247, 248, 250, 251, 252, 253, 254, 255, 255, 256, 256, 256
};

static const uint8_t m_Atan[256] = {
    0,  0,  0,  0,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  3,  3,  3,
    3,  3,  3,  3,  4,  4,  4,  4,  4,  4,  5,  5,  5,  5,  5,  5,  6,  6,  6,
    6,  6,  6,  6,  7,  7,  7,  7,  7,  7,  8,  8,  8,  8,  8,  8,  8,  9,  9,
    9,  9,  9,  9,  10, 10, 10, 10, 10, 10, 10, 11, 11, 11, 11, 11, 11, 11, 12,
    12, 12, 12, 12, 12, 12, 13, 13, 13, 13, 13, 13, 13, 14, 14, 14, 14, 14, 14,
    14, 15, 15, 15, 15, 15, 15, 15, 16, 16, 16, 16, 16, 16, 16, 17, 17, 17, 17,
    17, 17, 17, 17, 18, 18, 18, 18, 18, 18, 18, 19, 19, 19, 19, 19, 19, 19, 19,
    20, 20, 20, 20, 20, 20, 20, 20, 21, 21, 21, 21, 21, 21, 21, 21, 21, 22, 22,
    22, 22, 22, 22, 22, 22, 23, 23, 23, 23, 23, 23, 23, 23, 23, 24, 24, 24, 24,
    24, 24, 24, 24, 24, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 26, 26, 26, 26,
    26, 26, 26, 26, 26, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 28, 28, 28, 28,
    28, 28, 28, 28, 28, 28, 28, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 30,
    30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 31, 31, 31, 31, 31, 31, 31, 31, 31,
    31, 31, 31, 32, 32, 32, 32, 32, 32
};

static int32_t M_DivideFloor(const int32_t value, const int32_t divisor)
{
    return value / divisor - (value % divisor < 0);
}

static int16_t M_Root(const uint32_t value)
{
    if (value > UINT32_C(0x3fff8000)) {
        return 0;
    }
    uint32_t remainder = value;
    uint32_t root = 0;
    for (uint32_t bit = UINT32_C(1) << 30; bit != 0; bit >>= 2) {
        if (remainder >= root + bit) {
            remainder -= root + bit;
            root = (root >> 1) + bit;
        } else {
            root >>= 1;
        }
    }
    return (int16_t)(root + (remainder > root));
}

int16_t Math16_Sin(const int32_t angle)
{
    const uint32_t phase = (uint32_t)angle & 255;
    const uint32_t offset = phase & 63;
    const int16_t value = m_Sine[(phase & 64) != 0 ? 64 - offset : offset];
    return (phase & 128) != 0 ? (int16_t)-value : value;
}

int16_t Math16_Cos(const int32_t angle)
{
    return Math16_Sin((int32_t)(((uint32_t)angle + 64) & 255));
}

int16_t Math16_Wrap(const int32_t value)
{
    const int32_t low = (int32_t)((uint32_t)value & 0xffff);
    return (int16_t)(low >= 0x8000 ? low - 0x10000 : low);
}

int16_t Math16_PolarX(
    const int16_t center, const int16_t length, const int32_t angle)
{
    return Math16_Wrap(center + M_DivideFloor(length * Math16_Cos(angle), 256));
}

int16_t Math16_PolarY(
    const int16_t center, const int16_t length, const int32_t angle)
{
    return Math16_Wrap(center + M_DivideFloor(length * Math16_Sin(angle), 256));
}

int16_t Math16_Wave(const int16_t amplitude, const int32_t angle)
{
    return Math16_Wrap(amplitude * Math16_Sin(angle) / 256);
}

uint8_t Math16_Angle(const int16_t y, const int16_t x)
{
    if (x == 0 && y == 0) {
        return 0;
    }
    const uint32_t ax = (uint32_t)(x < 0 ? -(int32_t)x : x);
    const uint32_t ay = (uint32_t)(y < 0 ? -(int32_t)y : y);
    int32_t angle;
    if (ax == ay) {
        angle = 32;
    } else if (ax > ay) {
        angle = m_Atan[ay * 256 / ax];
    } else {
        angle = 64 - m_Atan[ax * 256 / ay];
    }
    if (x < 0) {
        angle = 128 - angle;
    }
    if (y < 0) {
        angle = -angle;
    }
    return (uint8_t)angle;
}

MATH16_VECTOR Math16_Vector(const int16_t length, const int32_t angle)
{
    return (MATH16_VECTOR) { Math16_PolarX(0, length, angle),
                             Math16_PolarY(0, length, angle) };
}

MATH16_VECTOR Math16_Aim(
    const MATH16_VECTOR from, const MATH16_VECTOR to, const int16_t length,
    const uint8_t offset)
{
    const uint8_t angle =
        Math16_Angle(Math16_Wrap(to.y - from.y), Math16_Wrap(to.x - from.x));
    return Math16_Vector(length, (uint8_t)(angle + offset));
}

int16_t Math16_Sqrt(const int32_t value)
{
    return M_Root((uint32_t)value);
}

int16_t Math16_Hypot(const int16_t x, const int16_t y)
{
    return M_Root((uint32_t)(x * x) + (uint32_t)(y * y));
}

int16_t Math16_FromPixel(const int16_t pixel)
{
    return Math16_Wrap(pixel * 16);
}

int16_t Math16_ToPixel(const int16_t subpixel)
{
    return (int16_t)M_DivideFloor(subpixel, 16);
}
