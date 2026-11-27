#include "th02/bomb/private.h"

#include "core/base/assert.h"
#include "pc98/zun/math16.h"

static uint16_t M_Offset(
    const int16_t scroll, const int16_t left, const int16_t top)
{
    int16_t rolled = Math16_Wrap((int32_t)top + scroll);
    if (rolled >= 400) {
        rolled -= 400;
    }
    const uint16_t scaled = (uint16_t)((uint16_t)rolled << 6);
    const int32_t column = left / 8 - (left % 8 < 0);
    return (uint16_t)(column + scaled + (scaled >> 2));
}

static uint16_t M_NextRow(const uint16_t offset)
{
    uint16_t next = (uint16_t)(offset + 80);
    if ((int16_t)next >= 32000) {
        next -= 32000;
    }
    return next;
}

static void M_Byte(const uint16_t offset, const uint8_t mask)
{
    ASSERT((g_Grcg->mode & 0xf0) == 0xc0);
    uint8_t *const pixels =
        g_Video.pages[g_Video.drawing_page] + (size_t)(offset & 0x7fff) * 8;
    const uint8_t enabled = ~g_Grcg->mode & 15;
    for (uint8_t bit = 0; bit < 8; bit++) {
        const uint8_t dot = 0x80u >> bit;
        if ((mask & dot) == 0) {
            continue;
        }
        uint8_t color = 0;
        for (uint8_t plane = 0; plane < 4; plane++) {
            if ((g_Grcg->tiles[plane] & dot) != 0) {
                color |= 1u << plane;
            }
        }
        pixels[bit] = (pixels[bit] & ~enabled) | (color & enabled);
    }
}

static void M_Block(uint16_t offset)
{
    for (size_t row = 0; row < 16; row++) {
        M_Byte(offset, 255);
        M_Byte((uint16_t)(offset + 1), 255);
        offset = M_NextRow(offset);
    }
}

void BombRender_CirclePoint(
    const BOMB_ASSETS *const assets, const int16_t scroll, const int16_t left,
    const int16_t top)
{
    if (left <= 24 || left >= 416 || top <= 8 || top >= 384) {
        return;
    }
    uint16_t offset = M_Offset(scroll, left, top);
    const uint8_t shift = (uint16_t)left & 7;
    for (size_t row = 0; row < 8; row++) {
        const uint16_t bits = assets->particles[4][row];
        const uint16_t rotated =
            (uint16_t)((bits >> shift) | (bits << (16 - shift)));
        M_Byte(offset, (uint8_t)rotated);
        M_Byte((uint16_t)(offset + 1), (uint8_t)(rotated >> 8));
        offset = M_NextRow(offset);
    }
}

void BombRender_Particle(
    const BOMB_ASSETS *const assets, const int16_t scroll, const int16_t left,
    const int16_t top, const uint8_t image)
{
    ASSERT(image < 4);
    uint16_t offset = M_Offset(scroll, left, top);
    for (size_t row = 0; row < 8; row++) {
        M_Byte(offset, assets->particles[image][row]);
        offset = M_NextRow(offset);
    }
}

void BombRender_Smear(
    const BOMB_ASSETS *const assets, const int16_t scroll, const int16_t left,
    const int16_t bottom)
{
    ASSERT(bottom <= 384);
    uint16_t offset = M_Offset(scroll, left, 16);
    int16_t y = 16;
    size_t row = 4;
    uint8_t mask = 255;
    do {
        M_Byte(offset, mask);
        offset = M_NextRow(offset);
        y++;
        if (y >= bottom) {
            mask = assets->particles[4][row++];
        }
    } while (row < 8);
}

void BombRender_Tiles(
    const int16_t scroll, const int16_t left, const int16_t top,
    const uint8_t bits)
{
    uint16_t offset = M_Offset(scroll, left, top);
    for (uint8_t bit = 0; bit < 8; bit++) {
        if ((bits & (0x80u >> bit)) != 0) {
            M_Block(offset);
        }
        offset = (uint16_t)(offset + 2);
    }
}

void BombRender_Transition(const int16_t scroll, const uint8_t frame[72])
{
    for (int16_t row = 0; row < 24; row++) {
        for (int16_t group = 0; group < 3; group++) {
            BombRender_Tiles(
                scroll, 32 + group * 128, 16 + row * 16,
                frame[row * 3 + group]);
        }
    }
}

void BombRender_Text(const BOMB_ASSETS *const assets, const uint8_t frame)
{
    ASSERT(assets->shot_type == 1 && frame < 18);
    const uint8_t *source = assets->text[frame];
    const TRAM_CHARACTER space = { .code = 0x20 };
    for (uint16_t y = 1; y <= 23; y++) {
        for (uint16_t x = 4; x < 52; x += 4) {
            for (uint16_t half = 0; half < 2; half++) {
                const uint8_t mask = half == 0 ? 0xf0 : 0x0f;
                const uint16_t attribute = (*source & mask) != 0 ? 0x45 : 0xe1;
                Tram_PutCharacter(x + half * 2, y, &space, attribute);
                Tram_PutCharacter(x + half * 2 + 1, y, &space, attribute);
            }
            source++;
        }
    }
}

void BombRender_Illustration(
    const BOMB_ASSETS *const assets, const int16_t scroll, const int16_t left,
    const int16_t top)
{
    ASSERT(g_Grcg->mode == 0);
    const PI_IMAGE *const image = &assets->illustration;
    int16_t rolled = Math16_Wrap((int32_t)top + scroll);
    if (rolled >= 400) {
        rolled -= 400;
    }
    Video_PutImage8(
        g_Clip, image->pixels, (int16_t)image->width, (int16_t)image->height,
        left, rolled);
}
