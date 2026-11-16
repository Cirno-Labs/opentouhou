#include "th01/video.h"

#include "core/base/assert.h"

#include <string.h>

typedef struct {
    int32_t sx, sy, dx, dy, width, height;
} VIDEO_REGION;

static int64_t M_Max(const int64_t a, const int64_t b)
{
    return a > b ? a : b;
}

static int64_t M_Min(const int64_t a, const int64_t b)
{
    return a < b ? a : b;
}

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
    const int64_t left = M_Max(M_Max(-(int64_t)source.x, -(int64_t)x), 0);
    const int64_t top = M_Max(M_Max(-(int64_t)source.y, -(int64_t)y), 0);
    const int64_t right = M_Min(
        M_Min((int64_t)source_width - source.x, (int64_t)destination_width - x),
        source.width);
    const int64_t bottom = M_Min(
        M_Min(
            (int64_t)source_height - source.y, (int64_t)destination_height - y),
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

static void M_FlushLine(
    VIDEO *const video, const int32_t column, const int32_t row,
    const uint8_t mask, const uint8_t color)
{
    for (int32_t i = 0; i < 8; i++) {
        if ((mask & (0x80 >> i)) != 0) {
            Video_Pixel(video, column * 8 + i, row, color);
        }
    }
}

void Video_Init(VIDEO *const video, const ALLOCATOR *const allocator)
{
    uint8_t *const pages = Allocator_AllocZ(allocator, 2 * VIDEO_PAGE_SIZE);
    *video = (VIDEO) { .pages = { pages, pages + VIDEO_PAGE_SIZE } };
    for (int32_t i = 0; i < 16; i++) {
        video->palette[i].alpha = 255;
    }
}

void Video_SelectDrawingPage(VIDEO *const video, const int32_t page)
{
    ASSERT(page >= 0 && page < 2);
    video->drawing_page = page;
}

void Video_SelectVisiblePage(VIDEO *const video, const int32_t page)
{
    ASSERT(page >= 0 && page < 2);
    video->visible_page = page;
}

void Video_Clear(VIDEO *const video, const uint8_t color)
{
    memset(video->pages[video->drawing_page], color & 15, VIDEO_PAGE_SIZE);
}

void Video_Pixel(
    VIDEO *const video, const int32_t x, const int32_t y, const uint8_t color)
{
    if (x >= 0 && x < VIDEO_WIDTH && y >= 0 && y < VIDEO_HEIGHT) {
        video->pages[video->drawing_page][y * VIDEO_WIDTH + x] = color & 15;
    }
}

uint8_t Video_ReadPixel(
    const VIDEO *const video, const int32_t page, const int32_t x,
    const int32_t y)
{
    ASSERT(page >= 0 && page < 2);
    if (x < 0 || x >= VIDEO_WIDTH || y < 0 || y >= VIDEO_HEIGHT) {
        return 0;
    }
    return video->pages[page][y * VIDEO_WIDTH + x];
}

void Video_HorizontalLine(
    VIDEO *const video, int32_t left, int32_t right, const int32_t y,
    const uint8_t color)
{
    M_Order(&left, &right);
    left = (int32_t)M_Max(left, 0);
    right = (int32_t)M_Min(right, VIDEO_WIDTH - 1);
    if (left <= right && y >= 0 && y < VIDEO_HEIGHT) {
        memset(
            video->pages[video->drawing_page] + y * VIDEO_WIDTH + left,
            color & 15, (size_t)(right - left + 1));
    }
}

void Video_VerticalLine(
    VIDEO *const video, const int32_t x, int32_t top, int32_t bottom,
    const uint8_t color)
{
    M_Order(&top, &bottom);
    top = (int32_t)M_Max(top, 0);
    bottom = (int32_t)M_Min(bottom, VIDEO_HEIGHT - 1);
    if (x >= 0 && x < VIDEO_WIDTH) {
        for (int32_t y = top; y <= bottom; y++) {
            Video_Pixel(video, x, y, color);
        }
    }
}

void Video_FillBox(
    VIDEO *const video, int32_t left, int32_t top, int32_t right,
    int32_t bottom, const uint8_t color)
{
    M_Order(&left, &right);
    M_Order(&top, &bottom);
    top = (int32_t)M_Max(top, 0);
    bottom = (int32_t)M_Min(bottom, VIDEO_HEIGHT - 1);
    for (int32_t y = top; y <= bottom; y++) {
        Video_HorizontalLine(video, left, right, y, color);
    }
}

void Video_Box(
    VIDEO *const video, const int32_t left, const int32_t top,
    const int32_t right, const int32_t bottom, const uint8_t color)
{
    Video_HorizontalLine(video, left, right, top, color);
    Video_VerticalLine(video, left, top, bottom, color);
    Video_VerticalLine(video, right, top, bottom, color);
    Video_HorizontalLine(video, left, right, bottom, color);
}

void Video_Line(
    VIDEO *const video, int32_t x1, int32_t y1, int32_t x2, int32_t y2,
    const uint8_t color)
{
    ASSERT(x1 >= INT16_MIN && x1 <= INT16_MAX);
    ASSERT(x2 >= INT16_MIN && x2 <= INT16_MAX);
    ASSERT(y1 >= INT16_MIN && y1 <= INT16_MAX);
    ASSERT(y2 >= INT16_MIN && y2 <= INT16_MAX);
    if (x1 == x2) {
        Video_VerticalLine(video, x1, y1, y2, color);
        return;
    }
    if (y1 == y2) {
        Video_HorizontalLine(video, x1, x2, y1, color);
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
        y1 += ((y2 - y1) / (x2 - x1)) * -x1;
        x1 = 0;
    }
    if (x2 >= VIDEO_WIDTH) {
        if (x1 >= VIDEO_WIDTH) {
            return;
        }
        y2 -= ((y2 - y1) / (x2 - x1)) * (x2 - (VIDEO_WIDTH - 1));
        x2 = VIDEO_WIDTH - 1;
    }
    if (y1 < 0) {
        if (y2 < 0) {
            return;
        }
        x1 += ((x2 - x1) / (y2 - y1)) * -y1;
        y1 = 0;
    }
    if (y2 >= VIDEO_HEIGHT) {
        if (y1 >= VIDEO_HEIGHT) {
            return;
        }
        x2 -= ((x2 - x1) / (y2 - y1)) * (y2 - (VIDEO_HEIGHT - 1));
        y2 = VIDEO_HEIGHT - 1;
    }
    if (y2 < 0) {
        x2 += ((x2 - x1) / (y2 - y1)) * -y2;
        y2 = 0;
    }
    if (y1 >= VIDEO_HEIGHT) {
        x1 -= ((x2 - x1) / (y2 - y1)) * (y1 - (VIDEO_HEIGHT - 1));
        y1 = VIDEO_HEIGHT - 1;
    }
    const int32_t direction = y1 < y2 ? 1 : -1;
    const int32_t dx = x2 - x1;
    const int32_t dy = (y2 - y1) * direction;
    const int32_t length = dx > dy ? dx : dy;
    int32_t error = length / 2;
    int32_t x = x1;
    int32_t y = y1;
    int32_t column = x / 8;
    int32_t row = y;
    uint8_t mask = 0;
    for (int32_t i = 0; i <= length; i++) {
        if (x / 8 != column || y != row) {
            M_FlushLine(video, column, row, mask, color);
            mask = 0;
            column = x / 8;
            row = y;
        }
        mask |= (uint8_t)(0x80 >> (x & 7));
        error -= dx > dy ? dy : dx;
        if (dx > dy) {
            x++;
        } else {
            y += direction;
        }
        if (error < 0) {
            error += length;
            if (dx > dy) {
                y += direction;
            } else {
                x++;
            }
        }
    }
}

void Video_Blit(
    VIDEO *const video, const VIDEO_IMAGE *const image, const GFX_RECT source,
    const int32_t x, const int32_t y)
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
        uint8_t *const destination = video->pages[video->drawing_page]
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
    VIDEO *const video, const VIDEO_MASK *const mask, const GFX_RECT source,
    const int32_t x, const int32_t y, const uint8_t color)
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
        uint8_t *const destination = video->pages[video->drawing_page]
            + (region.dy + row) * VIDEO_WIDTH + region.dx;
        for (int32_t col = 0; col < region.width; col++) {
            if (pixels[col] != 0) {
                destination[col] = color & 15;
            }
        }
    }
}

void Video_Copy(
    VIDEO *const video, const int32_t source_page, const GFX_RECT source,
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
            video->pages[destination_page] + (region.dy + row) * VIDEO_WIDTH
                + region.dx,
            video->pages[source_page] + (region.sy + row) * VIDEO_WIDTH
                + region.sx,
            (size_t)region.width);
    }
}

void Video_Restore16(VIDEO *const video, const GFX_RECT rectangle)
{
    Video_SelectDrawingPage(video, 0);
    if (rectangle.width <= 0 || rectangle.height <= 0) {
        return;
    }
    const int64_t left = M_Max((int64_t)rectangle.x & ~INT64_C(15), 0);
    const int64_t right = M_Min(
        ((int64_t)rectangle.x + rectangle.width + 15) & ~INT64_C(15),
        VIDEO_WIDTH);
    const int64_t top = M_Max(rectangle.y, 0);
    const int64_t bottom =
        M_Min((int64_t)rectangle.y + rectangle.height, VIDEO_HEIGHT);
    if (left < right && top < bottom) {
        const GFX_RECT source = { (int32_t)left, (int32_t)top,
                                  (int32_t)(right - left),
                                  (int32_t)(bottom - top) };
        Video_Copy(video, 1, source, 0, source.x, source.y);
    }
}

void Video_Read(
    const VIDEO *const video, const int32_t page, const GFX_RECT source,
    uint8_t *const pixels, const int32_t stride)
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
            video->pages[page] + (region.sy + row) * VIDEO_WIDTH + region.sx,
            (size_t)region.width);
    }
}

void Video_SetDac(
    VIDEO *const video, const int32_t index, const int32_t red,
    const int32_t green, const int32_t blue)
{
    ASSERT(index >= 0 && index < 16);
    video->palette[index] =
        (GFX_COLOR) { (uint8_t)((red & 15) * 17), (uint8_t)((green & 15) * 17),
                      (uint8_t)((blue & 15) * 17), 255 };
}

void Video_SetPalette(VIDEO *const video, const GFX_COLOR palette[16])
{
    for (int32_t i = 0; i < 16; i++) {
        Video_SetDac(
            video, i, palette[i].red >> 4, palette[i].green >> 4,
            palette[i].blue >> 4);
    }
}

void Video_SetScroll(VIDEO *const video, const int16_t top)
{
    const int16_t reduced = top % VIDEO_HEIGHT;
    video->scroll_top = reduced < 0 ? reduced + VIDEO_HEIGHT : reduced;
}

bool Video_Invert32(
    VIDEO *const video, const int16_t left, const int16_t top,
    const uint8_t plane_bit)
{
    ASSERT(
        plane_bit == 1 || plane_bit == 2 || plane_bit == 4 || plane_bit == 8);
    if (left < 0 || top < 0 || left >= 608 || top >= 368) {
        return false;
    }
    for (int32_t y = top; y < top + 32; y++) {
        for (int32_t x = left / 8 * 8; x < left / 8 * 8 + 32; x++) {
            video->pages[video->drawing_page][y * VIDEO_WIDTH + x] ^= plane_bit;
        }
    }
    return true;
}
