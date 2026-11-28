// Stands in for src/th02/draw/sprites.c: a test that links this file gets
// these definitions, so the linker never pulls the real sprites object
// from th02_lib. It covers every function draw/sprites.h declares.
//
// The real draws leave the charger in a documented state afterward (off,
// with tiles set); the fake reproduces that through the real Grcg_*
// functions so later charger-dependent code in the same test still sees
// the state the header promises, without approximating the clip math
// that decides whether an off-screen Draw_SpriteRect "drew" at all.

#include "tests/th02/fake_draw.h"
#include "th02/draw/sprites.h"
#include "th02/grcg.h"

static void M_Tiles(const uint8_t tile)
{
    Grcg_Off();
    Grcg_SetTiles(tile);
}

void Draw_Sprite(
    const PATTERN *const pattern, const int16_t left, const int16_t top)
{
    TestDraw_Record((TEST_DRAW_CALL) {
        .op = TEST_DRAW_SPRITE,
        .a = left,
        .b = top,
        .pattern = pattern,
    });
    M_Tiles(255);
}

void Draw_SpriteRoll(
    const PATTERN *const pattern, const int16_t left, const int16_t top)
{
    TestDraw_Record((TEST_DRAW_CALL) {
        .op = TEST_DRAW_SPRITE_ROLL,
        .a = left,
        .b = top,
        .pattern = pattern,
    });
    M_Tiles(255);
}

void Draw_SpriteRect(
    const PATTERN *const pattern, const int16_t left, const int16_t top)
{
    TestDraw_Record((TEST_DRAW_CALL) {
        .op = TEST_DRAW_SPRITE_RECT,
        .a = left,
        .b = top,
        .pattern = pattern,
    });
    M_Tiles(255);
}

void Draw_SpriteRollTiny(
    const PATTERN *const pattern, const int16_t left, const int16_t top)
{
    TestDraw_Record((TEST_DRAW_CALL) {
        .op = TEST_DRAW_SPRITE_ROLL_TINY,
        .a = left,
        .b = top,
        .pattern = pattern,
    });
    // Replays the real color switches recorded in the pattern, the way
    // Blit_RollPutTiny's caller leaves the charger.
    size_t offset = 0;
    while (pattern->data[offset] == 0x80) {
        Grcg_SetColor(pattern->data[offset + 1]);
        Grcg_Off();
        offset += 34;
    }
}

void Draw_SpritePlane(
    const PATTERN *const pattern, const int16_t left, const int16_t top,
    const uint8_t source_plane, const uint16_t mode)
{
    TestDraw_Record((TEST_DRAW_CALL) {
        .op = TEST_DRAW_SPRITE_PLANE,
        .a = left,
        .b = top,
        .e = mode,
        .plane = source_plane,
        .pattern = pattern,
    });
    M_Tiles((uint8_t)(mode >> 8));
}

void Draw_SpriteRollPlane(
    const PATTERN *const pattern, const int16_t left, const int16_t top,
    const uint8_t source_plane, const uint16_t mode)
{
    TestDraw_Record((TEST_DRAW_CALL) {
        .op = TEST_DRAW_SPRITE_ROLL_PLANE,
        .a = left,
        .b = top,
        .e = mode,
        .plane = source_plane,
        .pattern = pattern,
    });
    M_Tiles((uint8_t)(mode >> 8));
}

void Draw_SpriteZoom(
    const PATTERN *const pattern, const int16_t left, const int16_t top,
    const int16_t scale)
{
    TestDraw_Record((TEST_DRAW_CALL) {
        .op = TEST_DRAW_SPRITE_ZOOM,
        .a = left,
        .b = top,
        .c = scale,
        .pattern = pattern,
    });
}

void Draw_SpriteOpaque(
    const PATTERN *const pattern, const int16_t left, const int16_t top)
{
    TestDraw_Record((TEST_DRAW_CALL) {
        .op = TEST_DRAW_SPRITE_OPAQUE,
        .a = left,
        .b = top,
        .pattern = pattern,
    });
}

void Draw_MaskByte(
    const uint32_t offset, const uint8_t mask, const uint8_t color)
{
    TestDraw_Record((TEST_DRAW_CALL) {
        .op = TEST_DRAW_MASK_BYTE,
        .a = (int32_t)offset,
        .color = color,
        .flag = mask != 0,
        .e = mask,
    });
}
