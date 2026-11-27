#include "th01/shape.h"

#include "th01/sprite.h"

#include "pc98/zun/math16.h"

static int32_t M_Byte(const int16_t x)
{
    return x >= 0 ? x / 8 : -((-((int32_t)x) + 7) / 8);
}

void Shape_EllipseArcPut(
    const int16_t center_x, const int16_t center_y, const int16_t radius_x,
    const int16_t radius_y, const uint8_t color, const uint8_t step,
    const uint8_t start, const uint8_t end)
{
    int32_t cached = -1;
    uint8_t dots = 0;
    if (step == 0) {
        return;
    }
    for (int32_t angle = start; angle <= end; angle += step) {
        const int16_t x = Math16_PolarX(center_x, radius_x, angle);
        const int16_t y = Math16_PolarY(center_y, radius_y, angle);
        const int32_t offset = Math16_Wrap(y * 80 + M_Byte(x));
        const uint8_t bit = (uint8_t)(0x80u >> ((uint16_t)x & 7));
        if (offset != cached) {
            if (x >= 0 && x < VIDEO_WIDTH && cached >= 0 && cached < 32000) {
                uint8_t *const row =
                    g_Video.pages[g_Video.drawing_page] + cached * 8;
                for (int32_t i = 0; i < 8; i++) {
                    if ((dots & (0x80u >> i)) != 0) {
                        row[i] = color & 15;
                    }
                }
            }
            cached = offset;
            dots = bit;
        } else {
            dots |= bit;
        }
    }
}

void Shape_EllipseArcErase(
    const int16_t center_x, const int16_t center_y, const int16_t radius_x,
    const int16_t radius_y, const uint8_t step, const uint8_t start,
    const uint8_t end)
{
    int32_t previous_byte = 0;
    int16_t previous_y = 0;
    bool first = true;
    if (step == 0) {
        return;
    }
    for (int32_t angle = start; angle <= end; angle += step) {
        const int16_t x = Math16_PolarX(center_x, radius_x, angle);
        const int16_t y = Math16_PolarY(center_y, radius_y, angle);
        if (first || y != previous_y || M_Byte(x) != previous_byte) {
            Sprite_Restore16(x, y, 8, 1);
            previous_byte = M_Byte(x);
            previous_y = y;
            first = false;
        }
    }
}
