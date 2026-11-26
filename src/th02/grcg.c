#include "th02/grcg.h"

#include "core/base/assert.h"

#include <string.h>

enum { M_LEFT = 1, M_RIGHT = 2, M_BOTTOM = 4, M_TOP = 8 };

static GRCG m_Charger;
const GRCG *const g_Charger = &m_Charger;

static int32_t M_Signed16(const int32_t value)
{
    const uint16_t bits = (uint16_t)value;
    return bits < 0x8000 ? bits : (int32_t)bits - 65536;
}

static void M_Check(const CLIP *const clip)
{
    ASSERT(
        clip->left >= 0 && clip->left <= clip->right
        && clip->right < VIDEO_WIDTH);
    ASSERT(
        clip->top >= 0 && clip->top <= clip->bottom
        && clip->bottom < VIDEO_HEIGHT);
    ASSERT(
        m_Charger.mode == 0 || (m_Charger.mode & 0xf0) == 0x80
        || (m_Charger.mode & 0xf0) == 0xc0);
}

static bool M_Axis(
    const int16_t a, const int16_t b, const int16_t low, const int16_t high,
    int32_t *const first, int32_t *const last)
{
    int32_t end = M_Signed16((a < b ? b : a) - low);
    if (end < 0) {
        return false;
    }
    int32_t start = M_Signed16((a < b ? a : b) - low);
    start = start < 0 ? 0 : start;
    end = end > high - low ? high - low : end;
    if (start > end) {
        return false;
    }
    *first = low + start;
    *last = low + end;
    return true;
}

static void M_WriteByte(const uint16_t offset, const uint8_t mask)
{
    for (uint8_t plane = 0; plane < 4; plane++) {
        if (m_Charger.mode == 0) {
            if (plane == 0) {
                Video_WritePlaneByte(plane, offset, mask);
            }
        } else if ((m_Charger.mode & (1u << plane)) == 0) {
            const uint8_t previous =
                Video_ReadPlaneByte(g_Video.drawing_page, plane, offset);
            const uint8_t bits = (m_Charger.mode & 0x40) != 0
                ? (previous & ~mask) | (m_Charger.tiles[plane] & mask)
                : m_Charger.tiles[plane];
            Video_WritePlaneByte(plane, offset, bits);
        }
    }
}

static void M_WriteWord(const uint16_t offset, const uint16_t mask)
{
    M_WriteByte(offset, mask >> 8);
    M_WriteByte(offset + 1, mask & 255);
}

static uint8_t M_OutCode(
    const CLIP *const clip, const int32_t x, const int32_t y)
{
    uint8_t code = 0;
    if (x < clip->left) {
        code |= M_LEFT;
    } else if (x > clip->right) {
        code |= M_RIGHT;
    }
    if (y < 0) {
        code |= M_TOP;
    } else if (y > clip->bottom - clip->top) {
        code |= M_BOTTOM;
    }
    return code;
}

static int32_t M_Intersect(
    const int32_t offset, const int32_t difference, const int32_t length)
{
    ASSERT(length != 0);
    const int32_t quotient = offset * difference / length;
    ASSERT(quotient >= INT16_MIN && quotient <= INT16_MAX);
    return quotient;
}

static void M_CutLine(
    const CLIP *const clip, int32_t *const x, int32_t *const y,
    uint8_t *const code, const int32_t end_x, const int32_t end_y)
{
    if (*code == 0) {
        return;
    }
    const int32_t height = clip->bottom - clip->top;
    int32_t edge;
    if ((*code & (M_LEFT | M_RIGHT)) != 0) {
        edge = (*code & M_LEFT) != 0 ? clip->left : clip->right;
        const int32_t dx = M_Signed16(*x - end_x);
        if (dx == 0) {
            return;
        }
        const int32_t dy = M_Signed16(*y - end_y);
        const int32_t offset = M_Signed16(edge - end_x);
        *y = M_Signed16(end_y + M_Intersect(offset, dy, dx));
        *x = edge;
        if (*y >= 0 && *y <= height) {
            *code = 0;
            return;
        }
        edge = *y < 0 ? 0 : height;
    } else {
        edge = (*code & M_BOTTOM) != 0 ? height : 0;
    }
    const int32_t dy = M_Signed16(*y - end_y);
    if (dy == 0) {
        return;
    }
    const int32_t dx = M_Signed16(*x - end_x);
    const int32_t offset = M_Signed16(edge - end_y);
    *x = M_Signed16(end_x + M_Intersect(offset, dx, dy));
    *y = edge;
    *code = M_OutCode(clip, *x, *y);
}

void Grcg_Init(void)
{
    memset(&m_Charger, 0, sizeof(m_Charger));
}

void Grcg_LoadPlane(const uint8_t plane, const uint8_t byte)
{
    m_Charger.tiles[plane] = byte;
}

void Grcg_FillTiles(const uint8_t tile)
{
    memset(m_Charger.tiles, tile, sizeof(m_Charger.tiles));
}

void Grcg_SetColor(const uint8_t color)
{
    m_Charger.mode = 0xc0;
    for (size_t plane = 0; plane < 4; plane++) {
        m_Charger.tiles[plane] = (color & (1u << plane)) != 0 ? 255 : 0;
    }
}

void Grcg_SetPlanes(const uint8_t planes)
{
    m_Charger.mode = 0xc0 | (~planes & 15);
}

void Grcg_Off(void)
{
    m_Charger.mode = 0;
}

void Grcg_BoxFill(
    const CLIP *const clip, const int16_t left, const int16_t top,
    const int16_t right, const int16_t bottom)
{
    M_Check(clip);
    int32_t x1, x2, y1, y2;
    if (!M_Axis(left, right, clip->left, clip->right, &x1, &x2)
        || !M_Axis(top, bottom, clip->top, clip->bottom, &y1, &y2)) {
        return;
    }
    for (int32_t y = y2; y >= y1; y--) {
        for (int32_t byte = x1 / 16 * 2; byte <= x2 / 16 * 2 + 1; byte++) {
            uint8_t mask = 0;
            for (int32_t bit = 0; bit < 8; bit++) {
                if (byte * 8 + bit >= x1 && byte * 8 + bit <= x2) {
                    mask |= 0x80u >> bit;
                }
            }
            const uint16_t offset = y * 80 + byte;
            M_WriteByte(offset, mask);
        }
    }
}

void Grcg_HLine(
    const CLIP *const clip, const int16_t left, const int16_t right,
    const int16_t y)
{
    M_Check(clip);
    if ((uint16_t)((uint16_t)y - (uint16_t)clip->top)
        > clip->bottom - clip->top) {
        return;
    }
    int32_t first = M_Signed16(left - clip->left);
    int32_t last = M_Signed16(right - clip->left);
    if (first < 0 && last < 0) {
        return;
    }
    if (first > last) {
        const int32_t swap = first;
        first = last;
        last = swap;
    }
    first = first < 0 ? 0 : first;
    last = last > clip->right - clip->left ? clip->right - clip->left : last;
    if (first <= last) {
        Grcg_BoxFill(clip, clip->left + first, y, clip->left + last, y);
    }
}

void Grcg_VLine(
    const CLIP *const clip, const int16_t x, const int16_t top,
    const int16_t bottom)
{
    M_Check(clip);
    int32_t first, last;
    if (x < clip->left || x > clip->right
        || !M_Axis(top, bottom, clip->top, clip->bottom, &first, &last)) {
        return;
    }
    const uint8_t mask = 0x80u >> (x & 7);
    for (int32_t y = first; y <= last; y++) {
        M_WriteByte(y * 80 + x / 8, mask);
    }
}

void Grcg_Line(
    const CLIP *const clip, const int16_t left, const int16_t top,
    const int16_t right, const int16_t bottom)
{
    M_Check(clip);
    int32_t x1 = left, x2 = right;
    int32_t y1 = M_Signed16(top - clip->top);
    int32_t y2 = M_Signed16(bottom - clip->top);
    uint8_t first = M_OutCode(clip, x1, y1);
    uint8_t last = M_OutCode(clip, x2, y2);
    if ((first & last) != 0) {
        return;
    }
    M_CutLine(clip, &x1, &y1, &first, x2, y2);
    if ((first | last) != 0) {
        M_CutLine(clip, &x2, &y2, &last, x1, y1);
        if ((first | last) != 0) {
            return;
        }
    }
    if (x1 > x2) {
        const int32_t x = x1, y = y1;
        x1 = x2;
        y1 = y2;
        x2 = x;
        y2 = y;
    }
    y1 += clip->top;
    y2 += clip->top;
    const int32_t dx = x2 - x1;
    const int32_t step_y = y2 < y1 ? -1 : 1;
    const int32_t dy = (y2 - y1) * step_y;
    uint32_t error = 0x8000;
    if (dx <= dy) {
        const uint32_t step = dx == dy ? 65536u : (uint32_t)dx * 65536u / dy;
        for (int32_t i = 0; i <= dy; i++) {
            M_WriteByte(y1 * 80 + x1 / 8, 0x80u >> (x1 & 7));
            error += step;
            x1 += error >> 16;
            error &= 65535;
            y1 += step_y;
        }
    } else {
        const uint32_t step = (uint32_t)dy * 65536u / dx;
        uint16_t address = y1 * 80 + x1 / 16 * 2;
        uint16_t mask = 0;
        for (int32_t x = x1; x <= x2; x++) {
            const uint16_t next = y1 * 80 + x / 16 * 2;
            if (next != address) {
                M_WriteWord(address, mask);
                address = next;
                mask = 0;
            }
            mask |= 0x8000u >> (x & 15);
            error += step;
            if (error > UINT16_MAX) {
                y1 += step_y;
                error &= 65535;
            }
        }
        M_WriteWord(address, mask);
    }
}

void Grcg_RoundBoxFill(
    const CLIP *const clip, const int16_t left, const int16_t top,
    const int16_t right, const int16_t bottom, const uint16_t radius)
{
    M_Check(clip);
    int32_t x1 = left < right ? left : right;
    int32_t x2 = left < right ? right : left;
    int32_t y1 = top < bottom ? top : bottom;
    int32_t y2 = top < bottom ? bottom : top;
    const int32_t horizontal = (x2 - x1) / 2;
    const int32_t vertical = (y2 - y1) / 2;
    int32_t r = radius < horizontal ? radius : horizontal;
    r = r < vertical ? r : vertical;
    if (r == 0) {
        Grcg_BoxFill(clip, left, top, right, bottom);
        return;
    }
    y1 += r;
    y2 -= r;
    if (y2 - 1 >= y1 + 1) {
        Grcg_BoxFill(clip, x1, y1 + 1, x2, y2 - 1);
    }
    x1 += r;
    x2 -= r;
    int32_t wx = r, wy = 0, error = r;
    do {
        Grcg_HLine(clip, x1 - wx, x2 + wx, y1 - wy);
        Grcg_HLine(clip, x1 - wx, x2 + wx, y2 + wy);
        error = M_Signed16(error - (wy * 2 + 1));
        if (error < 0) {
            Grcg_HLine(clip, x1 - wy, x2 + wy, y1 - wx);
            Grcg_HLine(clip, x1 - wy, x2 + wy, y2 + wx);
            wx--;
            error = M_Signed16(error + wx * 2);
        }
        wy++;
    } while (wx >= wy);
}
