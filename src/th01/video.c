#include "th01/video.h"

#include "core/base/assert.h"
#include "core/base/utils.h"
#include "pc98/zun/math16.h"

#include <string.h>

typedef struct {
    int32_t sx;
    int32_t sy;
    int32_t dx;
    int32_t dy;
    int32_t width;
    int32_t height;
} VIDEO_REGION;

VIDEO g_Video;

static void M_Order(int32_t *const a, int32_t *const b)
{
    if (*a > *b) {
        const int32_t swap = *a;
        *a = *b;
        *b = swap;
    }
}

static bool M_Region(
    const GFX_RECT source, const int32_t source_width,
    const int32_t source_height, const int32_t x, const int32_t y,
    const int32_t destination_width, const int32_t destination_height,
    VIDEO_REGION *const out)
{
    const int64_t left = MAX(MAX(-(int64_t)source.x, -(int64_t)x), 0);
    const int64_t top = MAX(MAX(-(int64_t)source.y, -(int64_t)y), 0);
    const int64_t right = MIN(
        MIN((int64_t)source_width - source.x, (int64_t)destination_width - x),
        source.width);
    const int64_t bottom = MIN(
        MIN((int64_t)source_height - source.y, (int64_t)destination_height - y),
        source.height);
    if (left >= right || top >= bottom) {
        return false;
    }
    *out = (VIDEO_REGION) {
        .sx = (int32_t)(source.x + left),
        .sy = (int32_t)(source.y + top),
        .dx = (int32_t)(x + left),
        .dy = (int32_t)(y + top),
        .width = (int32_t)(right - left),
        .height = (int32_t)(bottom - top),
    };
    return true;
}

static int32_t M_LineColumn(const int32_t x)
{
    return x / 8 - (x % 8 < 0);
}

static int16_t M_LineLerp(
    const int32_t numerator, const int32_t denominator, const int32_t distance)
{
    const int32_t slope = Math16_Wrap(numerator) / Math16_Wrap(denominator);
    return Math16_Wrap(slope * Math16_Wrap(distance));
}

static uint16_t M_LinePattern(const uint16_t pattern, const int32_t x)
{
    const int32_t shift = x & 7;
    return (uint16_t)((uint32_t)pattern >> shift
                      | (uint32_t)pattern << (16 - shift));
}

static void M_FlushLine(
    const int32_t column, const int32_t row, const uint16_t mask,
    const uint8_t color)
{
    for (int32_t byte = 0; byte < 2; byte++) {
        const uint32_t offset = (uint32_t)(row * 80 + column + byte) & 0x7fffu;
        if (offset >= 32000) {
            continue;
        }
        for (int32_t bit = 0; bit < 8; bit++) {
            if ((mask & (0x80u << (byte * 8) >> bit)) != 0) {
                g_Video.pages[g_Video.drawing_page][offset * 8 + bit] =
                    color & 15;
            }
        }
    }
}

static void M_RestoreRawLine(const uint16_t offset)
{
    Video_SelectDrawingPage(0);
    for (uint32_t byte = offset; byte < (uint32_t)offset + 4; byte++) {
        uint32_t planar = byte;
        uint8_t mask = 15;
        if (byte >= 65536) {
            planar -= 65536;
            mask = 4;
        } else if (byte >= 32768) {
            planar -= 32768;
            mask = 6;
        }
        if (planar >= 32000) {
            continue;
        }
        for (int32_t bit = 0; bit < 8; bit++) {
            const uint32_t pixel = planar * 8 + bit;
            g_Video.pages[0][pixel] =
                (uint8_t)((g_Video.pages[0][pixel] & ~mask)
                          | (g_Video.pages[1][pixel] & mask));
        }
    }
}

static void M_Line(
    int32_t x1, int32_t y1, int32_t x2, int32_t y2, const uint8_t color,
    const uint16_t pattern, const bool erase)
{
    ASSERT(x1 >= INT16_MIN && x1 <= INT16_MAX);
    ASSERT(x2 >= INT16_MIN && x2 <= INT16_MAX);
    ASSERT(y1 >= INT16_MIN && y1 <= INT16_MAX);
    ASSERT(y2 >= INT16_MIN && y2 <= INT16_MAX);
    if (!erase && x1 == x2) {
        M_Order(&y1, &y2);
        y1 = MAX(y1, 0);
        y2 = MIN(y2, VIDEO_HEIGHT - 1);
        const uint16_t mask = M_LinePattern(pattern, x1);
        for (int32_t y = y1; y <= y2; y++) {
            M_FlushLine(M_LineColumn(x1), y, mask, color);
        }
        return;
    }
    if (!erase && y1 == y2) {
        M_Order(&x1, &x2);
        x1 = MAX(x1, 0);
        x2 = MIN(x2, VIDEO_WIDTH - 1);
        for (int32_t x = x1; x <= x2; x++) {
            M_FlushLine(
                M_LineColumn(x), y1, (uint16_t)(0x80u >> (x & 7)), color);
        }
        return;
    }
    if (x1 > x2) {
        M_Order(&x1, &x2);
        const int32_t swap = y1;
        y1 = y2;
        y2 = swap;
    }
    if (x1 < 0) {
        if (x2 < 0) {
            return;
        }
        y1 = Math16_Wrap(y1 + M_LineLerp(y2 - y1, x2 - x1, -x1));
        x1 = 0;
    }
    if (x2 >= VIDEO_WIDTH) {
        if (x1 >= VIDEO_WIDTH) {
            return;
        }
        y2 = Math16_Wrap(
            y2 - M_LineLerp(y2 - y1, x2 - x1, x2 - (VIDEO_WIDTH - 1)));
        x2 = VIDEO_WIDTH - 1;
    }
    if (y1 < 0) {
        if (y2 < 0) {
            return;
        }
        x1 = Math16_Wrap(x1 + M_LineLerp(x2 - x1, y2 - y1, -y1));
        y1 = 0;
    }
    if (y2 >= VIDEO_HEIGHT) {
        if (y1 >= VIDEO_HEIGHT) {
            return;
        }
        x2 = Math16_Wrap(
            x2 - M_LineLerp(x2 - x1, y2 - y1, y2 - (VIDEO_HEIGHT - 1)));
        y2 = VIDEO_HEIGHT - 1;
    }
    if (y2 < 0) {
        x2 = Math16_Wrap(x2 + M_LineLerp(x2 - x1, y2 - y1, -y2));
        y2 = 0;
    }
    if (y1 >= VIDEO_HEIGHT) {
        x1 = Math16_Wrap(
            x1 - M_LineLerp(x2 - x1, y2 - y1, y1 - (VIDEO_HEIGHT - 1)));
        y1 = VIDEO_HEIGHT - 1;
    }
    const int32_t direction = y1 < y2 ? 1 : -1;
    const int32_t dx = Math16_Wrap(x2 - x1);
    const int32_t dy = Math16_Wrap((y2 - y1) * direction);
    const int32_t length = dx > dy ? dx : dy;
    int32_t error = length / 2;
    int32_t x = x1;
    int32_t y = y1;
    int32_t column = M_LineColumn(x);
    int32_t row = y;
    uint16_t mask = 0;
    for (int32_t i = 0; i <= length; i++) {
        if (M_LineColumn(x) != column || y != row) {
            if (erase) {
                M_RestoreRawLine((uint16_t)(row * 80 + column - 1));
            } else {
                M_FlushLine(column, row, mask, color);
            }
            mask = 0;
            column = M_LineColumn(x);
            row = y;
        }
        mask |= M_LinePattern(pattern, x);
        error = Math16_Wrap(error - (dx > dy ? dy : dx));
        if (dx > dy) {
            x = Math16_Wrap(x + 1);
        } else {
            y = Math16_Wrap(y + direction);
        }
        if (error < 0) {
            error = Math16_Wrap(error + length);
            if (dx > dy) {
                y = Math16_Wrap(y + direction);
            } else {
                x = Math16_Wrap(x + 1);
            }
        }
    }
    if (erase) {
        M_RestoreRawLine((uint16_t)(y * 80 + M_LineColumn(x) - 1));
    }
}

void Video_Init(const ALLOCATOR *const allocator)
{
    uint8_t *const pages = Allocator_AllocZ(allocator, 2 * VIDEO_PAGE_SIZE);
    memset(&g_Video, 0, sizeof(g_Video));
    g_Video.pages[0] = pages;
    g_Video.pages[1] = pages + VIDEO_PAGE_SIZE;
    for (int32_t i = 0; i < 16; i++) {
        g_Video.palette[i].alpha = 255;
    }
}

void Video_SelectDrawingPage(const int32_t page)
{
    ASSERT(page >= 0 && page < 2);
    g_Video.drawing_page = page;
}

void Video_SelectVisiblePage(const int32_t page)
{
    ASSERT(page >= 0 && page < 2);
    g_Video.visible_page = page;
}

void Video_Clear(const uint8_t color)
{
    memset(g_Video.pages[g_Video.drawing_page], color & 15, VIDEO_PAGE_SIZE);
}

void Video_Pixel(const int32_t x, const int32_t y, const uint8_t color)
{
    if (x >= 0 && x < VIDEO_WIDTH && y >= 0 && y < VIDEO_HEIGHT) {
        g_Video.pages[g_Video.drawing_page][y * VIDEO_WIDTH + x] = color & 15;
    }
}

void Video_HorizontalLine(
    int32_t left, int32_t right, const int32_t y, const uint8_t color)
{
    M_Order(&left, &right);
    left = MAX(left, 0);
    right = MIN(right, VIDEO_WIDTH - 1);
    if (left <= right && y >= 0 && y < VIDEO_HEIGHT) {
        memset(
            g_Video.pages[g_Video.drawing_page] + y * VIDEO_WIDTH + left,
            color & 15, (size_t)(right - left + 1));
    }
}

void Video_VerticalLine(
    const int32_t x, int32_t top, int32_t bottom, const uint8_t color)
{
    M_Order(&top, &bottom);
    top = MAX(top, 0);
    bottom = MIN(bottom, VIDEO_HEIGHT - 1);
    if (x >= 0 && x < VIDEO_WIDTH) {
        for (int32_t y = top; y <= bottom; y++) {
            Video_Pixel(x, y, color);
        }
    }
}

void Video_FillBox(
    int32_t left, int32_t top, int32_t right, int32_t bottom,
    const uint8_t color)
{
    M_Order(&left, &right);
    M_Order(&top, &bottom);
    top = MAX(top, 0);
    bottom = MIN(bottom, VIDEO_HEIGHT - 1);
    for (int32_t y = top; y <= bottom; y++) {
        Video_HorizontalLine(left, right, y, color);
    }
}

void Video_Line(
    const int32_t x1, const int32_t y1, const int32_t x2, const int32_t y2,
    const uint8_t color)
{
    M_Line(x1, y1, x2, y2, color, 0x80, false);
}

void Video_LinePatterned(
    const int32_t x1, const int32_t y1, const int32_t x2, const int32_t y2,
    const uint8_t color, const uint16_t pattern)
{
    M_Line(x1, y1, x2, y2, color, pattern, false);
}

void Video_LineErase(
    const int32_t x1, const int32_t y1, const int32_t x2, const int32_t y2)
{
    M_Line(x1, y1, x2, y2, 0, 0x80, true);
}

void Video_Blit(
    const VIDEO_IMAGE *const image, const GFX_RECT source, const int32_t x,
    const int32_t y)
{
    ASSERT(image->width >= 0 && image->height >= 0);
    ASSERT(image->stride >= image->width);
    ASSERT(image->key >= -1 && image->key < 16);
    ASSERT(
        image->coverage == nullptr || image->coverage_stride >= image->width);
    VIDEO_REGION region;
    if (!M_Region(
            source, image->width, image->height, x, y, VIDEO_WIDTH,
            VIDEO_HEIGHT, &region)) {
        return;
    }
    for (int32_t row = 0; row < region.height; row++) {
        const size_t offset =
            (size_t)(region.sy + row) * image->stride + region.sx;
        const uint8_t *const pixels = image->pixels + offset;
        const uint8_t *const coverage = image->coverage == nullptr
            ? nullptr
            : image->coverage
                + (size_t)(region.sy + row) * image->coverage_stride
                + region.sx;
        uint8_t *const destination = g_Video.pages[g_Video.drawing_page]
            + (region.dy + row) * VIDEO_WIDTH + region.dx;
        for (int32_t col = 0; col < region.width; col++) {
            if (pixels[col] != image->key
                && (coverage == nullptr || coverage[col] != 0)) {
                destination[col] = pixels[col];
            }
        }
    }
}

void Video_Mask(
    const VIDEO_MASK *const mask, const GFX_RECT source, const int32_t x,
    const int32_t y, const uint8_t color)
{
    ASSERT(mask->width >= 0 && mask->height >= 0);
    ASSERT(mask->stride >= mask->width);
    VIDEO_REGION region;
    if (!M_Region(
            source, mask->width, mask->height, x, y, VIDEO_WIDTH, VIDEO_HEIGHT,
            &region)) {
        return;
    }
    for (int32_t row = 0; row < region.height; row++) {
        const uint8_t *const pixels =
            mask->pixels + (size_t)(region.sy + row) * mask->stride + region.sx;
        uint8_t *const destination = g_Video.pages[g_Video.drawing_page]
            + (region.dy + row) * VIDEO_WIDTH + region.dx;
        for (int32_t col = 0; col < region.width; col++) {
            if (pixels[col] != 0) {
                destination[col] = color & 15;
            }
        }
    }
}

void Video_Copy(
    const int32_t source_page, const GFX_RECT source,
    const int32_t destination_page, const int32_t x, const int32_t y)
{
    ASSERT(source_page >= 0 && source_page < 2);
    ASSERT(destination_page >= 0 && destination_page < 2);
    VIDEO_REGION region;
    if (!M_Region(
            source, VIDEO_WIDTH, VIDEO_HEIGHT, x, y, VIDEO_WIDTH, VIDEO_HEIGHT,
            &region)) {
        return;
    }
    const bool backwards =
        source_page == destination_page && region.dy > region.sy;
    for (int32_t i = 0; i < region.height; i++) {
        const int32_t row = backwards ? region.height - 1 - i : i;
        memmove(
            g_Video.pages[destination_page] + (region.dy + row) * VIDEO_WIDTH
                + region.dx,
            g_Video.pages[source_page] + (region.sy + row) * VIDEO_WIDTH
                + region.sx,
            (size_t)region.width);
    }
}

void Video_Restore16(const GFX_RECT rectangle)
{
    Video_SelectDrawingPage(0);
    if (rectangle.width <= 0 || rectangle.height <= 0) {
        return;
    }
    const int64_t left = MAX((int64_t)rectangle.x & ~INT64_C(15), 0);
    const int64_t right =
        MIN(((int64_t)rectangle.x + rectangle.width + 15) & ~INT64_C(15),
            VIDEO_WIDTH);
    const int64_t top = MAX(rectangle.y, 0);
    const int64_t bottom =
        MIN((int64_t)rectangle.y + rectangle.height, VIDEO_HEIGHT);
    if (left < right && top < bottom) {
        const GFX_RECT source = { (int32_t)left, (int32_t)top,
                                  (int32_t)(right - left),
                                  (int32_t)(bottom - top) };
        Video_Copy(1, source, 0, source.x, source.y);
    }
}

void Video_Read(
    const int32_t page, const GFX_RECT source, uint8_t *const pixels,
    const int32_t stride)
{
    ASSERT(page >= 0 && page < 2);
    ASSERT(source.width <= 0 || stride >= source.width);
    VIDEO_REGION region;
    if (!M_Region(
            source, VIDEO_WIDTH, VIDEO_HEIGHT, 0, 0, source.width,
            source.height, &region)) {
        return;
    }
    for (int32_t row = 0; row < region.height; row++) {
        memcpy(
            pixels + (size_t)(region.dy + row) * stride + region.dx,
            g_Video.pages[page] + (region.sy + row) * VIDEO_WIDTH + region.sx,
            (size_t)region.width);
    }
}

void Video_SetDac(
    const int32_t index, const int32_t red, const int32_t green,
    const int32_t blue)
{
    ASSERT(index >= 0 && index < 16);
    g_Video.palette[index] =
        (GFX_COLOR) { (uint8_t)((red & 15) * 17), (uint8_t)((green & 15) * 17),
                      (uint8_t)((blue & 15) * 17), 255 };
}

void Video_SetPalette(const GFX_COLOR palette[16])
{
    for (int32_t i = 0; i < 16; i++) {
        Video_SetDac(
            i, palette[i].red >> 4, palette[i].green >> 4,
            palette[i].blue >> 4);
    }
}

void Video_SetScroll(const int16_t top)
{
    const int16_t reduced = top % VIDEO_HEIGHT;
    g_Video.scroll_top = reduced < 0 ? reduced + VIDEO_HEIGHT : reduced;
}

bool Video_Invert32(
    const int16_t left, const int16_t top, const uint8_t plane_bit)
{
    ASSERT(
        plane_bit == 1 || plane_bit == 2 || plane_bit == 4 || plane_bit == 8);
    if (left < 0 || top < 0 || left >= 608 || top >= 368) {
        return false;
    }
    for (int32_t y = top; y < top + 32; y++) {
        for (int32_t x = left / 8 * 8; x < left / 8 * 8 + 32; x++) {
            g_Video.pages[g_Video.drawing_page][y * VIDEO_WIDTH + x] ^=
                plane_bit;
        }
    }
    return true;
}
