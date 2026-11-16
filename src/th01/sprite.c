#include "th01/sprite.h"

static int32_t M_Floor(const int32_t value, const int32_t unit)
{
    return value - (value % unit + unit) % unit;
}

static void M_Write(
    VIDEO *const video, const int32_t offset, const uint8_t color,
    const bool erase)
{
    if (offset >= 0 && offset < VIDEO_PAGE_SIZE) {
        video->pages[video->drawing_page][offset] =
            erase ? video->pages[1][offset] : color;
    }
}

static void M_Ptn(
    VIDEO *const video, const PTN *const sprites, const int32_t image,
    const int32_t quarter, const int16_t left, const int16_t top,
    const bool erase)
{
    const int32_t size = quarter < 0 ? 32 : 16;
    const int32_t sx = quarter < 0 ? 0 : (quarter & 1) * 16;
    const int32_t sy = quarter < 0 ? 0 : (quarter & 2) * 8;
    const uint8_t *const pixels = sprites->pixels + image * 1024;
    if (erase) {
        Video_SelectDrawingPage(video, 0);
    }
    for (int32_t y = 0; y < size; y++) {
        for (int32_t x = 0; x < size; x++) {
            const uint8_t color = pixels[(y + sy) * 32 + x + sx];
            if (color != PTN_TRANSPARENT) {
                M_Write(
                    video, (top + y) * 640 + M_Floor(left, 8) + x, color,
                    erase);
            }
        }
    }
}

static void M_PlayerRow(
    VIDEO *const video, const BOS *const sprites, const int32_t image,
    const int32_t y, const int32_t row, const int16_t left, const bool erase)
{
    const int32_t intended = row / 80 + (left < 0);
    for (int32_t x = 0; x < sprites->width; x++) {
        const int32_t byte = row + x / 8;
        const int32_t pixel =
            (image * sprites->height + y) * sprites->width + x;
        if (byte >= 0 && byte / 80 == intended
            && sprites->coverage[pixel] != 0) {
            M_Write(video, byte * 8 + x % 8, sprites->pixels[pixel], erase);
        }
    }
}

static void M_Player(
    VIDEO *const video, const BOS *const sprites, const int32_t image,
    const int16_t left, const int16_t top, const bool erase)
{
    Video_SelectDrawingPage(video, 0);
    int32_t row = (left + 640) / 8 - 80 + top * 80;
    for (int32_t y = 0; y < sprites->height; y++) {
        M_PlayerRow(video, sprites, image, y, row, left, erase);
        row += 80;
        if (row >= 32000) {
            break;
        }
    }
}

void Sprite_PtnPut(
    VIDEO *const video, const PTN *const sprites, const int32_t image,
    const int32_t quarter, const int16_t left, const int16_t top)
{
    M_Ptn(video, sprites, image, quarter, left, top, false);
}

void Sprite_PtnErase(
    VIDEO *const video, const PTN *const sprites, const int32_t image,
    const int32_t quarter, const int16_t left, const int16_t top)
{
    M_Ptn(video, sprites, image, quarter, left, top, true);
}

void Sprite_Restore16(
    VIDEO *const video, const int16_t left, const int16_t top,
    const int16_t width, const int16_t height)
{
    Video_SelectDrawingPage(video, 0);
    for (int32_t y = 0; y < height; y++) {
        for (int32_t x = M_Floor(left, 16); x < left + width; x += 16) {
            for (int32_t bit = 0; bit < 16; bit++) {
                const uint32_t offset =
                    (uint32_t)((top + y) * 640 + x + bit) & 0x3ffffu;
                if (offset < VIDEO_PAGE_SIZE) {
                    video->pages[0][offset] = video->pages[1][offset];
                }
            }
        }
    }
}

void Sprite_PlayerPut(
    VIDEO *const video, const BOS *const sprites, const int32_t image,
    const int16_t left, const int16_t top)
{
    M_Player(video, sprites, image, left, top, false);
}

void Sprite_PlayerErase(
    VIDEO *const video, const BOS *const sprites, const int32_t image,
    const int16_t left, const int16_t top)
{
    M_Player(video, sprites, image, left, top, true);
}

void Sprite_PlayerOverlap(
    VIDEO *const video, const BOS *const sprites, const int32_t put_image,
    const int32_t erase_image, const int16_t put_left, const int16_t put_top,
    const int16_t erase_left, const int16_t erase_top)
{
    Video_SelectDrawingPage(video, 0);
    const int32_t distance =
        M_Floor(put_left + 32, 8) / 8 - M_Floor(erase_left + 32, 8) / 8;
    int32_t row = (put_left + 640) / 8 - 80 + put_top * 80;
    for (int32_t y = 0; y < sprites->height; y++) {
        for (int32_t x = erase_left < 0 ? 8 : 0; x < sprites->width; x++) {
            const int32_t old =
                (erase_image * sprites->height + y) * sprites->width + x;
            const int32_t new_x = x - distance * 8;
            const bool covered = new_x >= 0 && new_x < sprites->width
                && sprites->coverage
                        [(put_image * sprites->height + y) * sprites->width
                         + new_x]
                    != 0;
            if (!covered && sprites->coverage[old] != 0) {
                const int32_t offset = (erase_top + y) * 640
                    + (erase_left < 0 ? x - 8 : M_Floor(erase_left, 8) + x);
                M_Write(video, offset, 0, true);
            }
        }
        M_PlayerRow(video, sprites, put_image, y, row, put_left, false);
        row += 80;
        if (row >= 32000) {
            break;
        }
    }
}
