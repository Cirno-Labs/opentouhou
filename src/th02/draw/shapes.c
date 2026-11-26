#include "th02/draw/shapes.h"

#include "core/base/assert.h"

#include <string.h>

enum { M_LEFT = 1, M_RIGHT = 2, M_BOTTOM = 4, M_TOP = 8 };

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
        g_Grcg->mode == 0 || (g_Grcg->mode & 0xf0) == 0x80
        || (g_Grcg->mode & 0xf0) == 0xc0);
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

void Draw_BoxFill(
    const CLIP *const clip, const int16_t left, const int16_t top,
    const int16_t right, const int16_t bottom)
{
    M_Check(clip);
    int32_t x1;
    int32_t x2;
    int32_t y1;
    int32_t y2;
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
            Grcg_WriteByte(offset, mask);
        }
    }
}

void Draw_HLine(
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
        Draw_BoxFill(clip, clip->left + first, y, clip->left + last, y);
    }
}

void Draw_VLine(
    const CLIP *const clip, const int16_t x, const int16_t top,
    const int16_t bottom)
{
    M_Check(clip);
    int32_t first;
    int32_t last;
    if (x < clip->left || x > clip->right
        || !M_Axis(top, bottom, clip->top, clip->bottom, &first, &last)) {
        return;
    }
    const uint8_t mask = 0x80u >> (x & 7);
    for (int32_t y = first; y <= last; y++) {
        Grcg_WriteByte(y * 80 + x / 8, mask);
    }
}

void Draw_Line(
    const CLIP *const clip, const int16_t left, const int16_t top,
    const int16_t right, const int16_t bottom)
{
    M_Check(clip);
    int32_t x1 = left;
    int32_t x2 = right;
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
        const int32_t x = x1;
        const int32_t y = y1;
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
            Grcg_WriteByte(y1 * 80 + x1 / 8, 0x80u >> (x1 & 7));
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
                Grcg_WriteWord(address, mask);
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
        Grcg_WriteWord(address, mask);
    }
}

void Draw_RoundBoxFill(
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
        Draw_BoxFill(clip, left, top, right, bottom);
        return;
    }
    y1 += r;
    y2 -= r;
    if (y2 - 1 >= y1 + 1) {
        Draw_BoxFill(clip, x1, y1 + 1, x2, y2 - 1);
    }
    x1 += r;
    x2 -= r;
    int32_t wx = r;
    int32_t wy = 0;
    int32_t error = r;
    do {
        Draw_HLine(clip, x1 - wx, x2 + wx, y1 - wy);
        Draw_HLine(clip, x1 - wx, x2 + wx, y2 + wy);
        error = M_Signed16(error - (wy * 2 + 1));
        if (error < 0) {
            Draw_HLine(clip, x1 - wy, x2 + wy, y1 - wx);
            Draw_HLine(clip, x1 - wy, x2 + wy, y2 + wx);
            wx--;
            error = M_Signed16(error + wx * 2);
        }
        wy++;
    } while (wx >= wy);
}

typedef struct {
    int16_t x;
    int16_t step;
    uint16_t fraction;
    uint16_t error;
} EDGE;

static int16_t M_Divide(const int32_t numerator, const int16_t denominator)
{
    ASSERT(denominator != 0);
    const int32_t quotient = numerator / denominator;
    ASSERT(quotient >= INT16_MIN && quotient <= INT16_MAX);
    return (int16_t)quotient;
}

static EDGE M_Edge(const int16_t from, const int16_t to, int16_t height)
{
    if (height == 0) {
        height = 1;
    }
    const int16_t difference = Math16_Wrap((int32_t)to - from);
    const int16_t quotient = M_Divide(difference, height);
    const int32_t remainder = difference % height;
    const uint32_t magnitude = remainder < 0 ? -remainder : remainder;
    const uint32_t fraction = magnitude * 65536u / (uint16_t)height;
    ASSERT(fraction <= UINT16_MAX);
    return (EDGE) {
        .x = from,
        .step = Math16_Wrap((int32_t)quotient - (remainder < 0)),
        .fraction = (uint16_t)(remainder < 0 ? 0u - fraction : fraction),
        .error = 0x8000,
    };
}

static void M_Advance(EDGE *const edge)
{
    const uint32_t sum = (uint32_t)edge->error + edge->fraction;
    edge->error = (uint16_t)sum;
    edge->x = Math16_Wrap((int32_t)edge->x + edge->step + (sum > UINT16_MAX));
}

static void M_Span(
    const CLIP *const clip, const uint16_t row, const int16_t a,
    const int16_t b)
{
    if (row > (clip->bottom - clip->top) * 80) {
        return;
    }
    int16_t left = Math16_Wrap((int32_t)a - clip->left);
    int16_t right = Math16_Wrap((int32_t)b - clip->left);
    if (left < 0 && right < 0) {
        return;
    }
    if (left > right) {
        const int16_t swap = left;
        left = right;
        right = swap;
    }
    left = left < 0 ? 0 : left;
    const int16_t width = clip->right - clip->left;
    right = right > width ? width : right;
    if (right < left) {
        return;
    }
    left += clip->left;
    right += clip->left;
    for (int32_t byte = left / 16 * 2; byte <= right / 16 * 2 + 1; byte++) {
        uint8_t mask = 0;
        for (int32_t bit = 0; bit < 8; bit++) {
            if (byte * 8 + bit >= left && byte * 8 + bit <= right) {
                mask |= 0x80u >> bit;
            }
        }
        const uint16_t address = (uint16_t)(clip->top * 80 + row + byte);
        for (uint8_t plane = 0; plane < 4; plane++) {
            if (g_Grcg->mode == 0) {
                if (plane == 0) {
                    Video_WritePlaneByte(plane, address, mask);
                }
            } else if ((g_Grcg->mode & (1u << plane)) == 0) {
                const uint8_t old =
                    Video_ReadPlaneByte(g_Video.drawing_page, plane, address);
                const uint8_t value =
                    (old & ~mask) | (g_Grcg->tiles[plane] & mask);
                Video_WritePlaneByte(plane, address, value);
            }
        }
    }
}

static void M_Trapezoid(
    const CLIP *const clip, EDGE *const a, EDGE *const b, const int16_t top,
    int16_t rows)
{
    uint16_t row = (uint16_t)((uint16_t)(top - clip->top) * 80u);
    do {
        M_Span(clip, row, a->x, b->x);
        M_Advance(a);
        M_Advance(b);
        row = (uint16_t)(row + 80u);
        rows = Math16_Wrap((int32_t)rows - 1);
    } while (rows >= 0);
}

static int32_t M_Next(
    const int32_t index, const int32_t count, const int32_t step)
{
    const int32_t next = index + step;
    return next < 0 ? count - 1 : next == count ? 0 : next;
}

static EDGE M_ClipEdge(
    const MATH16_VECTOR from, const MATH16_VECTOR to, const int16_t top)
{
    int16_t x = from.x;
    if (from.y < top) {
        const int16_t delta_x = Math16_Wrap((int32_t)from.x - to.x);
        const int16_t delta_y = Math16_Wrap((int32_t)top - from.y);
        const int16_t height = Math16_Wrap((int32_t)from.y - to.y);
        x = Math16_Wrap(
            (int32_t)x + M_Divide((int32_t)delta_x * delta_y, height));
    }
    return M_Edge(x, to.x, Math16_Wrap((int32_t)to.y - top));
}

void Draw_PolygonFill(
    const CLIP *const clip, const MATH16_VECTOR *const vertices,
    const int32_t count)
{
    ASSERT(count >= 2 && count <= 16384);
    ASSERT(clip->left >= 0 && clip->left <= clip->right && clip->right < 640);
    ASSERT(clip->top >= 0 && clip->top <= clip->bottom && clip->bottom < 400);
    ASSERT(g_Grcg->mode == 0 || (g_Grcg->mode & 0xf0) == 0xc0);
    int32_t left = 0;
    int32_t right = 0;
    int16_t top = vertices[0].y;
    int16_t bottom = top;
    int16_t min_x = vertices[0].x;
    int16_t max_x = min_x;
    for (int32_t i = 1; i < count; i++) {
        const MATH16_VECTOR point = vertices[i];
        if (point.y < top) {
            top = point.y;
            left = right = i;
        } else if (point.y == top) {
            right = i;
        } else if (point.y > bottom) {
            bottom = point.y;
        }
        min_x = point.x < min_x ? point.x : min_x;
        max_x = point.x > max_x ? point.x : max_x;
    }
    if (min_x > clip->right || max_x < clip->left || top > clip->bottom
        || bottom < clip->top) {
        return;
    }
    bottom = bottom > clip->bottom ? clip->bottom : bottom;
    if (left == 0 && right == count - 1) {
        left = count - 1;
        right = 0;
    }
    int32_t previous_left;
    int32_t previous_right;
    int32_t steps = 0;
    do {
        ASSERT(steps < count);
        steps++;
        previous_left = left;
        left = M_Next(left, count, -1);
    } while (vertices[left].y < clip->top);
    steps = 0;
    do {
        ASSERT(steps < count);
        steps++;
        previous_right = right;
        right = M_Next(right, count, 1);
    } while (vertices[right].y < clip->top);
    top = vertices[previous_left].y < clip->top ? clip->top
                                                : vertices[previous_left].y;
    EDGE a = M_ClipEdge(vertices[previous_left], vertices[left], top);
    EDGE b = M_ClipEdge(vertices[previous_right], vertices[right], top);
    for (int32_t segment = 0;; segment++) {
        ASSERT(segment <= count * 2);
        const int16_t end = vertices[left].y < vertices[right].y
            ? vertices[left].y
            : vertices[right].y;
        if (bottom <= end) {
            M_Trapezoid(clip, &a, &b, top, Math16_Wrap((int32_t)bottom - top));
            return;
        }
        M_Trapezoid(clip, &a, &b, top, Math16_Wrap((int32_t)end - 1 - top));
        top = end;
        if (top == vertices[left].y) {
            previous_left = left;
            left = M_Next(left, count, -1);
            a = M_Edge(
                vertices[previous_left].x, vertices[left].x,
                Math16_Wrap((int32_t)vertices[left].y - top));
        }
        if (top == vertices[right].y) {
            previous_right = right;
            right = M_Next(right, count, 1);
            b = M_Edge(
                vertices[previous_right].x, vertices[right].x,
                Math16_Wrap((int32_t)vertices[right].y - top));
        }
    }
}
