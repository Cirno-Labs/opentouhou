// Stands in for src/th02/draw/glyphs.c: a test that links this file gets
// these definitions, so the linker never pulls the real glyphs object
// from th02_lib. It covers every function draw/glyphs.h declares.
//
// Each real draw ends by setting the charger to color and turning it off;
// the fake does the same through the real Grcg_* functions.

#include "tests/th02/fake_draw.h"
#include "th02/draw/glyphs.h"
#include "th02/grcg.h"

void Draw_Gaiji(
    const GAIJI *const bank, const int16_t left, const int16_t top,
    const uint8_t glyph, const uint8_t color, const bool carry)
{
    (void)bank;
    TestDraw_Record((TEST_DRAW_CALL) {
        .op = TEST_DRAW_GAIJI,
        .a = left,
        .b = top,
        .c = glyph,
        .color = color,
        .flag = carry,
    });
    Grcg_SetColor(color);
    Grcg_Off();
}

void Draw_GaijiString(
    const GAIJI *const bank, const int16_t left, const int16_t top,
    const int16_t step, const uint8_t *const glyphs, const uint8_t color)
{
    (void)bank;
    TestDraw_Record((TEST_DRAW_CALL) {
        .op = TEST_DRAW_GAIJI_STRING,
        .a = left,
        .b = top,
        .c = step,
        .color = color,
        .pattern = glyphs,
    });
    Grcg_SetColor(color);
    Grcg_Off();
}

void Draw_Text(
    const DRAW_TEXT_GLYPH *const glyphs, const int32_t count,
    const int16_t left, const int16_t top, const uint16_t col_and_fx)
{
    TestDraw_Record((TEST_DRAW_CALL) {
        .op = TEST_DRAW_TEXT,
        .a = left,
        .b = top,
        .c = count,
        .e = col_and_fx,
        .pattern = glyphs,
    });
    Grcg_SetColor((uint8_t)col_and_fx);
    Grcg_Off();
}
