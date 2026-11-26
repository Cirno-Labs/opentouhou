#include "th02/draw/private.h"
#include "th02/draw/shapes.h"

#include "core/base/assert.h"

static int32_t M_Signed16(const int32_t value)
{
    const uint16_t bits = (uint16_t)value;
    return bits < 0x8000 ? bits : (int32_t)bits - 65536;
}

static void M_DrawPlane(
    const PATTERN *const pattern, const int16_t left, const int16_t top,
    const uint8_t source_plane, const uint16_t mode, const bool roll)
{
    const size_t width = pattern->size >> 8;
    const size_t height = pattern->size & 255;
    const size_t plane_size = width * height;
    ASSERT(width >= 1 && width <= 8 && height != 0);
    ASSERT(source_plane < 5 && (mode & 0xf0) == 0xc0);
    ASSERT(pattern->data != nullptr && pattern->storage_size >= plane_size * 5);
    const uint8_t selected = (uint8_t)~mode & 15;
    const uint8_t tile = mode >> 8;
    const size_t shift = (uint16_t)left & 7;
    uint8_t *const page = g_Video.pages[g_Video.drawing_page];
    for (size_t y = 0; y < height; y++) {
        int32_t row = top + (int32_t)y;
        if (roll && row >= VIDEO_HEIGHT) {
            row -= VIDEO_HEIGHT;
        }
        const uint16_t start =
            (uint16_t)((uint16_t)row * 80u + ((uint16_t)left >> 3));
        for (size_t x = 0; x < width * 8; x++) {
            const uint8_t bits =
                pattern->data[source_plane * plane_size + y * width + x / 8];
            if ((bits & (0x80u >> (x % 8))) != 0) {
                const size_t bit = (x + shift) % 8;
                const size_t pixel =
                    ((start + (x + shift) / 8) % VIDEO_PLANE_SIZE) * 8 + bit;
                page[pixel] = (page[pixel] & ~selected)
                    | ((tile & (0x80u >> bit)) != 0 ? selected : 0);
            }
        }
    }
}

static void M_Draw(
    const PATTERN *const pattern, const int16_t left, const int16_t top,
    const bool roll)
{
    const size_t byte_width = pattern->size >> 8;
    const size_t height = pattern->size & 255;
    const size_t plane_size = byte_width * height;
    ASSERT(byte_width >= 1 && byte_width <= 8 && height != 0);
    ASSERT(pattern->data != nullptr && pattern->storage_size >= plane_size * 5);
    const uint16_t column = (uint16_t)left >> 3;
    const size_t shift = (uint16_t)left & 7;
    uint8_t *const page = g_Video.pages[g_Video.drawing_page];
    for (size_t y = 0; y < height; y++) {
        int32_t row = top + (int32_t)y;
        if (roll && row >= VIDEO_HEIGHT) {
            row -= VIDEO_HEIGHT;
        }
        const uint16_t row_offset = (uint16_t)((uint16_t)row * 80u + column);
        for (size_t x = 0; x < byte_width * 8; x++) {
            const size_t source = y * byte_width + x / 8;
            const uint8_t bit = 0x80u >> (x % 8);
            uint8_t color = 0;
            for (size_t plane = 0; plane < 4; plane++) {
                if ((pattern->data[(plane + 1) * plane_size + source] & bit)
                    != 0) {
                    color |= 1u << plane;
                }
            }
            const size_t offset =
                ((row_offset + (x + shift) / 8) % VIDEO_PLANE_SIZE) * 8
                + (x + shift) % 8;
            const bool covered = (pattern->data[source] & bit) != 0;
            page[offset] = (covered ? 0 : page[offset]) | color;
        }
    }
}

void Blit_Put(
    const PATTERN *const pattern, const int16_t left, const int16_t top)
{
    M_Draw(pattern, left, top, false);
}

void Blit_RollPut(
    const PATTERN *const pattern, const int16_t left, const int16_t top)
{
    ASSERT(top < VIDEO_HEIGHT);
    M_Draw(pattern, left, top, true);
}

bool Blit_PutRect(
    const PATTERN *const pattern, const CLIP *const clip, const int16_t left,
    const int16_t top)
{
    const int32_t byte_width = pattern->size >> 8;
    const int32_t height = pattern->size & 255;
    const size_t plane_size = (size_t)byte_width * height;
    ASSERT(byte_width >= 1 && byte_width <= 8 && height != 0);
    ASSERT(pattern->data != nullptr && pattern->storage_size >= plane_size * 5);
    ASSERT(
        clip->left >= 0 && clip->left <= clip->right
        && clip->right < VIDEO_WIDTH);
    ASSERT(
        clip->top >= 0 && clip->top <= clip->bottom
        && clip->bottom < VIDEO_HEIGHT);
    if (M_Signed16(left + byte_width * 8 - 1) <= clip->right
        && left >= clip->left && top >= clip->top
        && M_Signed16(top + height) < clip->bottom) {
        Blit_Put(pattern, left, top);
        return true;
    }
    uint8_t *const page = g_Video.pages[g_Video.drawing_page];
    bool drew = false;
    for (int32_t y = 0; y < height; y++) {
        const int32_t screen_y = top + y;
        if (screen_y < clip->top || screen_y > clip->bottom) {
            continue;
        }
        for (int32_t x = 0; x < byte_width * 8; x++) {
            const int32_t screen_x = left + x;
            if (screen_x < clip->left || screen_x > clip->right) {
                continue;
            }
            const size_t source = (size_t)y * byte_width + x / 8;
            drew = true;
            const uint8_t bit = 0x80u >> (x % 8);
            uint8_t color = 0;
            for (size_t plane = 0; plane < 4; plane++) {
                if ((pattern->data[(plane + 1) * plane_size + source] & bit)
                    != 0) {
                    color |= 1u << plane;
                }
            }
            const size_t offset = (size_t)screen_y * VIDEO_WIDTH + screen_x;
            const bool covered = (pattern->data[source] & bit) != 0;
            page[offset] = (covered ? 0 : page[offset]) | color;
        }
    }
    return drew;
}

void Blit_RollPutTiny(
    const PATTERN *const pattern, const int16_t left, const int16_t top)
{
    ASSERT(top >= 0 && top < VIDEO_HEIGHT);
    ASSERT(pattern->size == 0x0210 && pattern->data != nullptr);
    ASSERT(pattern->storage_size >= 2);
    uint8_t *const page = g_Video.pages[g_Video.drawing_page];
    size_t offset = 0;
    size_t records = 0;
    while (pattern->data[offset] == 0x80) {
        ASSERT(records < 4 && offset + 34 <= pattern->storage_size);
        const uint8_t color = pattern->data[offset + 1] & 15;
        uint16_t row_offset = (uint16_t)(top * 80u + ((uint16_t)left >> 3));
        bool roll = row_offset >= (VIDEO_HEIGHT - 16 + 1) * 80;
        const size_t shift = (uint16_t)left & 7;
        for (size_t y = 0; y < 16; y++) {
            for (size_t x = 0; x < 16; x++) {
                const uint8_t mask = pattern->data[offset + 2 + y * 2 + x / 8];
                if ((mask & (0x80u >> (x % 8))) != 0) {
                    const size_t pixel =
                        ((row_offset + (x + shift) / 8) % VIDEO_PLANE_SIZE) * 8
                        + (x + shift) % 8;
                    page[pixel] = color;
                }
            }
            row_offset += 80;
            if (roll && row_offset >= VIDEO_HEIGHT * 80) {
                row_offset -= VIDEO_HEIGHT * 80;
                roll = false;
            }
        }
        offset += 34;
        records++;
        ASSERT(offset + 2 <= pattern->storage_size);
    }
}

void Blit_PutPlane(
    const PATTERN *const pattern, const int16_t left, const int16_t top,
    const uint8_t source_plane, const uint16_t mode)
{
    M_DrawPlane(pattern, left, top, source_plane, mode, false);
}

void Blit_RollPutPlane(
    const PATTERN *const pattern, const int16_t left, const int16_t top,
    const uint8_t source_plane, const uint16_t mode)
{
    ASSERT(top >= 0 && top < VIDEO_HEIGHT);
    M_DrawPlane(pattern, left, top, source_plane, mode, true);
}

void Blit_Zoom(
    const PATTERN *const pattern, const CLIP *const clip, const int16_t left,
    const int16_t top, const int16_t scale)
{
    const size_t width = pattern->size >> 8;
    const size_t height = pattern->size & 255;
    const size_t plane_size = width * height;
    ASSERT(width >= 1 && width <= 8 && height != 0);
    ASSERT(pattern->data != nullptr && pattern->storage_size >= plane_size * 5);
    const int32_t end = M_Signed16((int32_t)scale - 1);
    int16_t draw_y = top;
    for (size_t y = 0; y < height; y++) {
        int16_t draw_x = left;
        for (size_t x = 0; x < width * 8; x++) {
            const size_t source = y * width + x / 8;
            const uint8_t bit = 0x80u >> (x % 8);
            uint8_t color = 0;
            for (size_t plane = 0; plane < 4; plane++) {
                if ((pattern->data[(plane + 1) * plane_size + source] & bit)
                    != 0) {
                    color |= 1u << plane;
                }
            }
            if (color != 0) {
                Grcg_SetColor(color);
                Draw_BoxFill(
                    clip, draw_x, draw_y, M_Signed16(draw_x + end),
                    M_Signed16(draw_y + end));
            }
            draw_x = M_Signed16(draw_x + scale);
        }
        draw_y = M_Signed16(draw_y + scale);
    }
    Grcg_Off();
}

void Blit_OverPut8(
    const PATTERN *const pattern, const int16_t left, const int16_t top)
{
    const size_t width = pattern->size >> 8;
    const size_t height = pattern->size & 255;
    const size_t plane_size = width * height;
    ASSERT(width >= 1 && width <= 8 && height != 0);
    ASSERT(pattern->data != nullptr && pattern->storage_size >= plane_size * 5);
    for (uint8_t plane = 0; plane < 4; plane++) {
        for (size_t y = 0; y < height; y++) {
            const uint16_t row =
                (uint16_t)(((uint16_t)top + y) * 80u + ((uint16_t)left >> 3));
            for (size_t byte = 0; byte < width; byte++) {
                uint16_t offset = (uint16_t)(row + byte);
                uint8_t destination = plane;
                if (offset >= VIDEO_PLANE_SIZE) {
                    if (plane >= 2) {
                        continue;
                    }
                    offset -= VIDEO_PLANE_SIZE;
                    destination++;
                }
                Video_WritePlaneByte(
                    destination, offset,
                    pattern->data[(plane + 1) * plane_size + y * width + byte]);
            }
        }
    }
}
