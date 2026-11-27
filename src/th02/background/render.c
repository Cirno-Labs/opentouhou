#include "th02/background/private.h"
#include "th02/grcg.h"
#include "th02/video.h"

#include "core/base/assert.h"
#include "pc98/zun/math16.h"

#include <string.h>

static void M_CheckWords(const uint16_t offset, const int16_t height)
{
    ASSERT(height > 0);
    ASSERT(
        (uint32_t)offset + (uint32_t)(height - 1) * 80 + 1 < VIDEO_PLANE_SIZE);
}

static uint8_t M_SourceTail(const size_t byte)
{
    if (byte < 2) {
        return (uint8_t)((uint16_t)p_Background->scroll.copy_top >> (byte * 8));
    }
    if (byte < 4) {
        return (uint8_t)((uint16_t)p_Background->scroll.copy_height
                         >> ((byte - 2) * 8));
    }
    if (byte < 28) {
        return p_Background->scroll.row[byte - 4];
    }
    return ((const uint8_t *)&p_Background->cache)[byte - 28];
}

void Render_CopyRows(
    const uint16_t destination, const uint16_t source, const int16_t height,
    const TILE_COPY_MODE mode)
{
    ASSERT(mode == TILE_COPY_ALL || mode == TILE_COPY_INTENSITY);
    M_CheckWords(source, height);
    M_CheckWords(destination, height);
    const uint8_t keep = mode == TILE_COPY_INTENSITY ? 7 : 0;
    const uint8_t copy = mode == TILE_COPY_INTENSITY ? 8 : 255;
    uint8_t *const page = g_Video.pages[g_Video.drawing_page];
    for (size_t row = 0; row < (size_t)height; row++) {
        uint8_t pixels[16];
        memcpy(pixels, page + ((size_t)source + row * 80) * 8, sizeof(pixels));
        for (uint8_t plane = 0; plane < 4; plane++) {
            Grcg_LoadTile(
                plane,
                Video_ReadPlaneByte(
                    g_Video.drawing_page, plane,
                    (uint16_t)(source + row * 80 + (source & 1))));
        }
        uint8_t *const output = page + ((size_t)destination + row * 80) * 8;
        const size_t swap = ((source ^ destination) & 1) * 8;
        for (size_t pixel = 0; pixel < 16; pixel++) {
            output[pixel] =
                (output[pixel] & keep) | (pixels[pixel ^ swap] & copy);
        }
    }
}

uint16_t Background_Source(const uint8_t image)
{
    if (image < TILE_SOURCE_COUNT) {
        return p_Background->sources[image];
    }
    const size_t byte = (size_t)(image - TILE_SOURCE_COUNT) * 2;
    return (uint16_t)(M_SourceTail(byte)
                      | (uint16_t)M_SourceTail(byte + 1) << 8);
}

void Render_CopyLines(
    const uint16_t destination, const uint8_t image, const int16_t top,
    const int16_t height, const TILE_COPY_MODE mode)
{
    const uint16_t shift = (uint16_t)((uint16_t)top << 6);
    const uint16_t source = Background_Source(image) + shift + (shift >> 2);
    Render_CopyRows(destination, source, height, mode);
}

void Render_Black(const uint16_t destination)
{
    M_CheckWords(destination, 16);
    ASSERT(g_Grcg->mode == 0xc0);
    uint8_t *const page = g_Video.pages[g_Video.drawing_page];
    for (size_t row = 0; row < 16; row++) {
        memset(page + ((size_t)destination + row * 80) * 8, 0, 16);
    }
}

void Background_SetTile(
    const uint8_t front, const uint8_t back, const int16_t left,
    const int16_t top, const uint8_t image)
{
    ASSERT(front < 2 && back < 2);
    const int16_t row = top / 16 - (top % 16 < 0);
    const int16_t relative_x = Math16_Wrap((int32_t)left - 32);
    const int16_t column = relative_x / 16 - (relative_x % 16 < 0);
    const int16_t index = Math16_Wrap((int32_t)row * TILE_COLUMNS + column);
    ASSERT(
        index >= -1
        && (int32_t)index + 1 < (int32_t)sizeof(p_Background->cache));
    p_Background->scroll.copy_top = 0;
    p_Background->scroll.copy_height = 16;
    ((uint8_t *)&p_Background->cache)[index + 1] = image;
    const uint16_t scaled = (uint16_t)((uint16_t)(row * 16) << 6);
    const uint16_t destination =
        (uint16_t)(left / 8 - (left % 8 < 0) + scaled + (scaled >> 2));
    const uint16_t source = Background_Source(image);
    Video_AccessPage(front);
    Render_CopyRows(destination, source, 16, TILE_COPY_ALL);
    Video_AccessPage(back);
    Render_CopyRows(destination, source, 16, TILE_COPY_ALL);
    Grcg_Off();
}
