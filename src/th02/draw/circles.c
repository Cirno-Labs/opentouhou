#include "th02/draw/circles.h"

#include "th02/clip.h"
#include "th02/draw/shapes.h"
#include "th02/grcg.h"
#include "th02/video.h"

#include "core/base/assert.h"
#include "pc98/zun/math16.h"

static void M_DotByte(const uint32_t address, const uint8_t mask)
{
    const uint16_t offset = address % VIDEO_PLANE_SIZE;
    if (g_Grcg->mode == 0) {
        Video_WritePlaneByte(address / VIDEO_PLANE_SIZE, offset, mask);
        return;
    }
    for (uint8_t plane = 0; plane < 4; plane++) {
        if ((g_Grcg->mode & (1u << plane)) != 0) {
            continue;
        }
        uint8_t bits = g_Grcg->tiles[plane];
        if ((g_Grcg->mode & 0x40) != 0) {
            const uint8_t previous =
                Video_ReadPlaneByte(g_Video.drawing_page, plane, offset);
            bits = (previous & ~mask) | (bits & mask);
        }
        Video_WritePlaneByte(plane, offset, bits);
    }
}

static void M_CirclePoint(const int x, const int relative_y)
{
    const uint32_t offset = (relative_y + g_Clip->top) * 80 + x / 8;
    M_DotByte(offset, 0x80u >> (x & 7));
}

static bool M_CirclePair(const int16_t x, const int dx, const int relative_y)
{
    const int16_t left = Math16_Wrap((int32_t)x - dx);
    if (dx != 0 && left >= g_Clip->left) {
        if (left > g_Clip->right) {
            return false;
        }
        M_CirclePoint(left, relative_y);
    }
    const int16_t right = Math16_Wrap((int32_t)x + dx);
    if (right >= g_Clip->left && right <= g_Clip->right) {
        M_CirclePoint(right, relative_y);
    }
    return true;
}

static void M_CirclePart(
    const int16_t x, const int16_t relative_y, const int dx, const int dy,
    const bool clipped)
{
    const int height = g_Clip->bottom - g_Clip->top;
    if (dy != 0) {
        const int16_t top = Math16_Wrap((int32_t)relative_y - dy);
        if (top > height) {
            return;
        }
        if (top >= 0 && !M_CirclePair(x, dx, top)) {
            return;
        }
    }
    const int16_t bottom = Math16_Wrap((int32_t)relative_y + dy);
    if (bottom >= 0 && bottom <= height - clipped) {
        M_CirclePair(x, dx, bottom);
    }
}

void Draw_Dot(const int16_t left, const int16_t top, const uint8_t size)
{
    ASSERT(size <= 5);
    const uint8_t mode = g_Grcg->mode;
    ASSERT(mode == 0 || (mode & 0xf0) == 0xc0);
    const uint16_t pattern = (uint8_t)(0xffu << (8 - size));
    const unsigned shift = (uint16_t)left & 7;
    const uint16_t mask =
        (uint16_t)((pattern >> shift) | (pattern << (16 - shift)));
    const uint16_t scaled = (uint16_t)((uint16_t)top << 6);
    uint16_t offset =
        (uint16_t)(left / 8 - (left % 8 < 0) + scaled + (scaled >> 2));
    for (unsigned row = 0; row < size; row++) {
        M_DotByte(offset, (uint8_t)mask);
        M_DotByte((uint32_t)offset + 1, (uint8_t)(mask >> 8));
        offset = (uint16_t)(offset + 80);
    }
}

void Draw_Circle(const int16_t x, const int16_t y, const uint16_t radius)
{
    ASSERT(radius <= 1020 && (g_Grcg->mode & 0xf0) == 0xc0);
    if (radius == 0) {
        return;
    }
    const CLIP *const clip = g_Clip;
    const int height = clip->bottom - clip->top;
    const int16_t relative_y = Math16_Wrap((int32_t)y - clip->top);
    const int16_t top = Math16_Wrap((int32_t)relative_y - radius);
    const int16_t bottom = Math16_Wrap((int32_t)relative_y + radius);
    const int16_t left = Math16_Wrap((int32_t)x - radius);
    const int16_t right = Math16_Wrap((int32_t)x + radius);
    if (top > height || bottom < 0 || left > clip->right
        || right < clip->left) {
        return;
    }
    const bool clipped = top < 0 || bottom > height
        || Math16_Wrap((int32_t)left - clip->left) < 0 || right > clip->right;
    int dx = radius;
    int dy = 0;
    int error = radius;
    do {
        M_CirclePart(x, relative_y, dx, dy, clipped);
        if (dx != dy) {
            M_CirclePart(x, relative_y, dy, dx, clipped);
        }
        error -= dy * 2 + 1;
        if (error < 0) {
            dx--;
            error += dx * 2;
        }
        dy++;
    } while (dx >= dy);
}

void Draw_CircleFill(const int16_t x, const int16_t y, const uint16_t radius)
{
    ASSERT(radius <= 1020);
    if (radius == 0) {
        if (x >= g_Clip->left && x <= g_Clip->right && y >= g_Clip->top
            && y <= g_Clip->bottom) {
            M_CirclePoint(x, y - g_Clip->top);
        }
        return;
    }
    int dx = radius;
    int dy = 0;
    int error = radius;
    do {
        Draw_HLine(
            g_Clip, Math16_Wrap((int32_t)x - dx), Math16_Wrap((int32_t)x + dx),
            Math16_Wrap((int32_t)y - dy));
        Draw_HLine(
            g_Clip, Math16_Wrap((int32_t)x - dx), Math16_Wrap((int32_t)x + dx),
            Math16_Wrap((int32_t)y + dy));
        error -= dy * 2 + 1;
        if (error < 0) {
            Draw_HLine(
                g_Clip, Math16_Wrap((int32_t)x - dy),
                Math16_Wrap((int32_t)x + dy), Math16_Wrap((int32_t)y - dx));
            Draw_HLine(
                g_Clip, Math16_Wrap((int32_t)x - dy),
                Math16_Wrap((int32_t)x + dy), Math16_Wrap((int32_t)y + dx));
            dx--;
            error += dx * 2;
        }
        dy++;
    } while (dx >= dy);
}
