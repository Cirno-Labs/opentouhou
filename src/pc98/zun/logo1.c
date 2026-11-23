#include "pc98/zun/logo1.h"
#include "pc98/zun/master_rng.h"
#include "pc98/zun/math16.h"
#include "pc98/zun/math16.h"
#include "pc98/zun/master_rng.h"

#include "core/base/assert.h"

#include <string.h>

#define M_WIDTH 640
#define M_HEIGHT 400

static const int16_t m_Sine7[65] = {
    0,   3,   6,   9,   12,  15,  18,  21,  24,  28,  31,  34,  37,
    40,  43,  46,  48,  51,  54,  57,  60,  63,  65,  68,  71,  73,
    76,  78,  81,  83,  85,  88,  90,  92,  94,  96,  98,  100, 102,
    104, 106, 108, 109, 111, 112, 114, 115, 117, 118, 119, 120, 121,
    122, 123, 124, 124, 125, 126, 126, 127, 127, 127, 127, 127, 127
};

static int32_t M_Sine(const int32_t angle, const int16_t quarter[65])
{
    const uint32_t phase = (uint32_t)angle & 255;
    const uint32_t offset = phase & 63;
    const int32_t value = quarter[(phase & 64) != 0 ? 64 - offset : offset];
    return (phase & 128) != 0 ? -value : value;
}

static int32_t M_Shift(const int32_t value, const int32_t bits)
{
    const int32_t divisor = 1 << bits;
    return value >= 0 ? value / divisor : -((-value + divisor - 1) / divisor);
}

static void M_Pixel(
    uint8_t *const page, const int32_t x, const int32_t y, const uint8_t color)
{
    if (x >= 96 && x <= 543 && y >= 100 && y <= 299) {
        page[y * M_WIDTH + x] = color;
    }
}

static void M_Line(
    uint8_t *const page, const int32_t left, const int32_t right,
    const int32_t y, const uint8_t color)
{
    if (y < 100 || y > 299 || right < 96 || left > 543) {
        return;
    }
    const int32_t x0 = left < 96 ? 96 : left;
    const int32_t x1 = right > 543 ? 543 : right;
    memset(page + y * M_WIDTH + x0, color, (size_t)(x1 - x0 + 1));
}

static void M_Circle(
    uint8_t *const page, const ZUN_LOGO1_POINT center, const uint8_t color)
{
    int32_t x = 96;
    int32_t y = 0;
    int32_t error = 96;
    while (x >= y) {
        M_Line(page, center.x - x, center.x + x, center.y - y, color);
        M_Line(page, center.x - x, center.x + x, center.y + y, color);
        error -= y * 2 + 1;
        if (error < 0) {
            M_Line(page, center.x - y, center.x + y, center.y - x, color);
            M_Line(page, center.x - y, center.x + y, center.y + x, color);
            x--;
            error += x * 2;
        }
        y++;
    }
}

static void M_Sprite(
    const ZUN_LOGO1 *const logo, uint8_t *const page,
    const uint8_t *const sprites, const int32_t x, const int32_t index,
    const bool wave)
{
    ASSERT(index >= 0 && index < 16);
    const uint8_t *const pixels = sprites + index * 64 * 16;
    const int32_t period = logo->period < 0 ? -logo->period : logo->period;
    for (int32_t row = 0; row < 16; row++) {
        int32_t offset = 0;
        if (wave) {
            const int32_t angle = row * 256 / period + logo->phase;
            const int32_t amplitude = logo->period < 0 && row % 2 != 0
                ? -logo->amplitude
                : logo->amplitude;
            offset = M_Shift(M_Sine(angle, m_Sine7) * amplitude, 7);
        }
        for (int32_t col = 0; col < 64; col++) {
            const uint8_t color = pixels[row * 64 + col];
            if (color != 0) {
                M_Pixel(page, x + offset + col, 192 + row, color);
            }
        }
    }
}

static void M_Animate(
    ZUN_LOGO1 *const logo, uint8_t *const page, const uint8_t *const sprites)
{
    const int32_t frame = logo->frame;
    if (frame < 50 || frame >= 170) {
        return;
    }
    if (frame == 55 || frame == 60 || frame == 65 || frame == 110
        || frame == 155 || frame == 160 || frame == 165) {
        logo->sprite += 2;
    }
    const bool wave = frame >= 90 && frame < 130;
    M_Sprite(logo, page, sprites, 256, logo->sprite, wave);
    M_Sprite(logo, page, sprites, 320, logo->sprite + 1, wave);
    if (wave) {
        const int32_t step = frame < 110 ? -1 : 1;
        logo->period += step;
        logo->phase = (logo->phase + 4) & 255;
        logo->amplitude -= step * 4;
    }
}

void ZunLogo1_Init(ZUN_LOGO1 *const logo, uint8_t *pages[2])
{
    *logo = (ZUN_LOGO1) {
        .random = 1, .period = 23, .star_phase = 64, .visible_page = 1
    };
    memset(pages[0], 0, M_WIDTH * M_HEIGHT);
    memset(pages[1], 0, M_WIDTH * M_HEIGHT);
    for (int32_t i = 0; i < 4; i++) {
        logo->circles[i] = (ZUN_LOGO1_POINT) { (i + 1) * 128, 320 - i * 80 };
        logo->velocity[i] = (ZUN_LOGO1_POINT) { -8, 8 };
    }
    for (int32_t i = 0; i < 50; i++) {
        logo->stars[i].x = (int32_t)(MasterRng_Next(&logo->random) % 640);
        logo->stars[i].y = (int32_t)(MasterRng_Next(&logo->random) % 400);
        logo->speeds[i] = (uint8_t)(MasterRng_Next(&logo->random) % 32 + 6);
    }
}

bool ZunLogo1_Tick(
    ZUN_LOGO1 *const logo, uint8_t *pages[2], const uint8_t *const sprites)
{
    if (logo->half_tick) {
        logo->visible_page = logo->drawing_page;
        logo->drawing_page ^= 1;
        logo->frame++;
        logo->half_tick = false;
        return logo->frame <= 180 || logo->tone > 0;
    }
    if (logo->frame > 180) {
        logo->tone -= 2;
    } else if (logo->tone < 100) {
        logo->tone += 2;
    }
    uint8_t *const page = pages[logo->drawing_page];
    for (int32_t y = 100; y <= 299; y++) {
        memset(page + y * M_WIDTH + 96, 0, 448);
    }
    for (int32_t i = 0; i < 50; i++) {
        ZUN_LOGO1_POINT *const star = &logo->stars[i];
        M_Pixel(page, star->x, star->y, 5);
        star->x += M_Shift(logo->speeds[i] * Math16_Cos(logo->star_phase), 8);
        star->y += M_Shift(logo->speeds[i] * Math16_Sin(logo->star_phase), 8);
        star->x = (star->x + M_WIDTH) % M_WIDTH;
        star->y = (star->y + M_HEIGHT) % M_HEIGHT;
    }
    logo->star_phase++;
    for (int32_t i = 3; i >= 0; i--) {
        ZUN_LOGO1_POINT *const circle = &logo->circles[i];
        ZUN_LOGO1_POINT *const velocity = &logo->velocity[i];
        M_Circle(page, *circle, (uint8_t)(4 - i));
        circle->x += velocity->x;
        circle->y += velocity->y;
        if (circle->x <= 32 || circle->x > 607) {
            velocity->x = -velocity->x;
        }
        if (circle->y <= 32 || circle->y > 367) {
            velocity->y = -velocity->y;
        }
    }
    M_Animate(logo, page, sprites);
    logo->half_tick = true;
    return true;
}

void ZunLogo1_Palette(
    const ZUN_LOGO1 *const logo, const GFX_COLOR source[16], GFX_COLOR out[16])
{
    for (int32_t i = 0; i < 16; i++) {
        const GFX_COLOR color = source[i];
        out[i] = (GFX_COLOR) {
            (uint8_t)(((color.red >> 4) * logo->tone / 100) * 17),
            (uint8_t)(((color.green >> 4) * logo->tone / 100) * 17),
            (uint8_t)(((color.blue >> 4) * logo->tone / 100) * 17), 255
        };
    }
}
