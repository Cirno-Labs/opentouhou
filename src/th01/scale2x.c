#include "th01/scale2x.h"

#include "core/base/assert.h"

void Scale2x_Copy(
    const int16_t destination_left, const int16_t destination_top,
    const int16_t source_left, const int16_t source_top, const int16_t width,
    const int16_t height)
{
    if (width < 16 || height <= 0) {
        return;
    }
    ASSERT(destination_left >= 0 && source_left >= 0);
    const int32_t x0 = destination_left / 8 * 8;
    const int32_t x1 = source_left / 8 * 8;
    const int32_t columns = width / 16 * 16;
    ASSERT(x0 + columns * 2 <= VIDEO_WIDTH && x1 + columns <= VIDEO_WIDTH);
    ASSERT(destination_top >= 0 && source_top >= 0);
    ASSERT(destination_top + height * 2 <= VIDEO_HEIGHT);
    ASSERT(source_top + height <= VIDEO_HEIGHT);
    for (int32_t y = 0; y < height; y++) {
        for (int32_t x = 0; x < columns; x++) {
            const uint8_t color =
                g_Video.pages[1][(source_top + y) * VIDEO_WIDTH + x1 + x];
            if (color != 0) {
                const int32_t at =
                    (destination_top + y * 2) * VIDEO_WIDTH + x0 + x * 2;
                g_Video.pages[0][at] = color;
                g_Video.pages[0][at + 1] = color;
                g_Video.pages[0][at + VIDEO_WIDTH] = color;
                g_Video.pages[0][at + VIDEO_WIDTH + 1] = color;
            }
        }
    }
    Video_SelectDrawingPage(0);
}
