#include "th02/video.h"

#include "core/base/assert.h"

#include <string.h>

VIDEO g_Video;

static void M_RestoreByte(uint8_t plane, uint32_t offset)
{
    if (offset >= VIDEO_PLANE_SIZE) {
        if (plane >= 2) {
            return;
        }
        plane++;
        offset -= VIDEO_PLANE_SIZE;
    }
    Video_WritePlaneByte(plane, offset, Video_ReadPlaneByte(1, plane, offset));
}

void Video_Init(const ALLOCATOR *const allocator)
{
    memset(&g_Video, 0, sizeof(g_Video));
    uint8_t *const pages = Allocator_AllocZ(allocator, VIDEO_PAGE_SIZE * 2);
    g_Video.pages[0] = pages;
    g_Video.pages[1] = pages + VIDEO_PAGE_SIZE;
    g_Video.first_lines = VIDEO_HEIGHT;
    for (size_t i = 0; i < 16; i++) {
        g_Video.palette[i].alpha = 255;
    }
}

void Video_AccessPage(const uint8_t page)
{
    g_Video.drawing_page = page & 1;
}

void Video_ShowPage(const uint8_t page)
{
    g_Video.visible_page = page & 1;
}

void Video_Clear(void)
{
    memset(g_Video.pages[g_Video.drawing_page], 0, VIDEO_VISIBLE_SIZE);
}

void Video_CopyPage(const uint8_t destination)
{
    Video_AccessPage(destination);
    memcpy(
        g_Video.pages[g_Video.drawing_page],
        g_Video.pages[g_Video.drawing_page ^ 1], VIDEO_VISIBLE_SIZE);
}

void Video_RestoreRect16(
    const int16_t left, const int16_t top, const int16_t width,
    const int16_t height)
{
    if (width < 16 || height <= 0) {
        return;
    }
    const int32_t x = left >= 0 ? left / 8 : -((-(int32_t)left + 7) / 8);
    uint16_t row = (uint16_t)((uint16_t)top * 80u + (uint16_t)x);
    for (int32_t y = 0; y < height; y++) {
        if (row <= INT16_MAX) {
            Video_AccessPage(0);
            for (int32_t word = 0; word < width / 16; word++) {
                const uint32_t offset = row + word * 2;
                if (offset + 1 < VIDEO_PLANE_SIZE) {
                    memcpy(
                        g_Video.pages[0] + offset * 8,
                        g_Video.pages[1] + offset * 8, 16);
                } else {
                    for (uint8_t plane = 0; plane < 4; plane++) {
                        M_RestoreByte(plane, offset);
                        M_RestoreByte(plane, offset + 1);
                    }
                }
            }
        }
        row = (uint16_t)(row + 80u);
    }
}

void Video_PutImage8(
    const CLIP *const clip, const uint8_t *const pixels, const int16_t width,
    const int16_t height, const int16_t left, const int16_t top)
{
    ASSERT(left >= 0 && top >= 0 && top < VIDEO_HEIGHT);
    ASSERT(width >= 0 && width % 2 == 0 && height >= 0);
    const int32_t x = left / 8 * 8;
    const int32_t count = width / 8 * 8;
    if (x >= VIDEO_WIDTH || count == 0) {
        return;
    }
    const size_t length = count < VIDEO_WIDTH - x ? count : VIDEO_WIDTH - x;
    int32_t y = top;
    for (int32_t row = 0; row < height; row++) {
        if (y >= clip->top && y <= clip->bottom) {
            memcpy(
                g_Video.pages[g_Video.drawing_page] + y * VIDEO_WIDTH + x,
                pixels + (size_t)row * width, length);
        }
        y++;
        if (y == VIDEO_HEIGHT) {
            y = 0;
        }
    }
}

uint8_t Video_ReadPlaneByte(
    const uint8_t page, const uint8_t plane, const uint16_t offset)
{
    ASSERT(plane < 4 && offset < VIDEO_PLANE_SIZE);
    const uint8_t *const pixels = g_Video.pages[page & 1] + (size_t)offset * 8;
    uint8_t bits = 0;
    for (size_t bit = 0; bit < 8; bit++) {
        bits |= ((pixels[bit] >> plane) & 1u) << (7 - bit);
    }
    return bits;
}

void Video_WritePlaneByte(
    const uint8_t plane, const uint16_t offset, const uint8_t bits)
{
    ASSERT(plane < 4 && offset < VIDEO_PLANE_SIZE);
    uint8_t *const pixels =
        g_Video.pages[g_Video.drawing_page] + (size_t)offset * 8;
    const uint8_t plane_bit = 1u << plane;
    for (size_t bit = 0; bit < 8; bit++) {
        pixels[bit] =
            (pixels[bit] & ~plane_bit) | (((bits >> (7 - bit)) & 1u) << plane);
    }
}

void Video_SetSplit(
    const uint16_t first_lines, const uint16_t first_address,
    const uint16_t second_address)
{
    g_Video.first_lines =
        first_lines < VIDEO_HEIGHT ? first_lines : VIDEO_HEIGHT;
    g_Video.first_address = first_address;
    g_Video.second_address = second_address;
}

void Video_ScrollUp(const uint16_t line)
{
    const uint16_t top = line < VIDEO_HEIGHT ? line : VIDEO_HEIGHT;
    Video_SetSplit(VIDEO_HEIGHT - top, top * 40, 0);
}

void Video_Scanout(uint8_t *const pixels, const size_t stride)
{
    ASSERT(stride >= VIDEO_WIDTH);
    const uint8_t *const page = g_Video.pages[g_Video.visible_page];
    for (size_t row = 0; row < VIDEO_HEIGHT; row++) {
        const bool second =
            g_Video.first_lines != 0 && row >= g_Video.first_lines;
        const size_t address =
            second ? g_Video.second_address : g_Video.first_address;
        const size_t source_row = second ? row - g_Video.first_lines : row;
        const size_t offset =
            (address * 16 + source_row * VIDEO_WIDTH) % VIDEO_PAGE_SIZE;
        const size_t first = VIDEO_PAGE_SIZE - offset < VIDEO_WIDTH
            ? VIDEO_PAGE_SIZE - offset
            : VIDEO_WIDTH;
        memcpy(pixels + row * stride, page + offset, first);
        if (first < VIDEO_WIDTH) {
            memcpy(pixels + row * stride + first, page, VIDEO_WIDTH - first);
        }
    }
}

void Video_SetDac(
    const uint8_t index, const uint8_t red, const uint8_t green,
    const uint8_t blue)
{
    ASSERT(index < 16);
    g_Video.palette[index] = (GFX_COLOR) {
        (uint8_t)((red & 15) * 17),
        (uint8_t)((green & 15) * 17),
        (uint8_t)((blue & 15) * 17),
        255,
    };
}
