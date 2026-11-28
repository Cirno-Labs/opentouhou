// Stands in for src/th02/draw/shapes.c: a test that links this file gets
// these definitions, so the linker never pulls the real shapes object
// from th02_lib. It covers every function draw/shapes.h declares.
//
// None of these touch the charger themselves (they paint through whatever
// mode and tiles the caller set up), so the fake only records their
// arguments.

#include "tests/th02/fake_draw.h"
#include "th02/draw/shapes.h"

void Draw_BoxFill(
    const CLIP *const clip, const int16_t left, const int16_t top,
    const int16_t right, const int16_t bottom)
{
    (void)clip;
    TestDraw_Record((TEST_DRAW_CALL) {
        .op = TEST_DRAW_BOX_FILL,
        .a = left,
        .b = top,
        .c = right,
        .d = bottom,
    });
}

void Draw_HLine(
    const CLIP *const clip, const int16_t left, const int16_t right,
    const int16_t y)
{
    (void)clip;
    TestDraw_Record((TEST_DRAW_CALL) {
        .op = TEST_DRAW_HLINE,
        .a = left,
        .b = y,
        .c = right,
    });
}

void Draw_VLine(
    const CLIP *const clip, const int16_t x, const int16_t top,
    const int16_t bottom)
{
    (void)clip;
    TestDraw_Record((TEST_DRAW_CALL) {
        .op = TEST_DRAW_VLINE,
        .a = x,
        .b = top,
        .d = bottom,
    });
}

void Draw_Line(
    const CLIP *const clip, const int16_t left, const int16_t top,
    const int16_t right, const int16_t bottom)
{
    (void)clip;
    TestDraw_Record((TEST_DRAW_CALL) {
        .op = TEST_DRAW_LINE,
        .a = left,
        .b = top,
        .c = right,
        .d = bottom,
    });
}

void Draw_RoundBoxFill(
    const CLIP *const clip, const int16_t left, const int16_t top,
    const int16_t right, const int16_t bottom, const uint16_t radius)
{
    (void)clip;
    TestDraw_Record((TEST_DRAW_CALL) {
        .op = TEST_DRAW_ROUND_BOX_FILL,
        .a = left,
        .b = top,
        .c = right,
        .d = bottom,
        .e = radius,
    });
}

void Draw_PolygonFill(
    const CLIP *const clip, const MATH16_VECTOR *const vertices,
    const int32_t count)
{
    (void)clip;
    TestDraw_Record((TEST_DRAW_CALL) {
        .op = TEST_DRAW_POLYGON_FILL,
        .a = count,
        .pattern = vertices,
    });
}
