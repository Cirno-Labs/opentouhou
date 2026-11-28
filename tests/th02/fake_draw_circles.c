// Stands in for src/th02/draw/circles.c: a test that links this file gets
// these definitions, so the linker never pulls the real circles object
// from th02_lib. It covers every function draw/circles.h declares.

#include "tests/th02/fake_draw.h"
#include "th02/draw/circles.h"
#include "th02/grcg.h"

#include <string.h>

TEST_DRAW_LOG g_TestDraw;

void TestDraw_Reset(void)
{
    memset(&g_TestDraw, 0, sizeof(g_TestDraw));
}

// The color the charger paints, one bit per plane.
uint8_t TestDraw_ChargerColor(void)
{
    uint8_t color = 0;
    for (int32_t plane = 0; plane < 4; plane++) {
        if (g_Grcg->tiles[plane] != 0) {
            color |= (uint8_t)(1 << plane);
        }
    }
    return color;
}

void TestDraw_Record(const TEST_DRAW_CALL call)
{
    if (g_TestDraw.count < TEST_DRAW_CALLS_MAX) {
        g_TestDraw.calls[g_TestDraw.count++] = call;
    }
}

void Draw_Dot(const int16_t left, const int16_t top, const uint8_t size)
{
    TestDraw_Record((TEST_DRAW_CALL) {
        .op = TEST_DRAW_DOT,
        .a = left,
        .b = top,
        .e = size,
        .color = TestDraw_ChargerColor(),
    });
}

void Draw_Circle(const int16_t x, const int16_t y, const uint16_t radius)
{
    TestDraw_Record((TEST_DRAW_CALL) {
        .op = TEST_DRAW_CIRCLE,
        .a = x,
        .b = y,
        .e = radius,
        .color = TestDraw_ChargerColor(),
    });
}

void Draw_CircleFill(const int16_t x, const int16_t y, const uint16_t radius)
{
    TestDraw_Record((TEST_DRAW_CALL) {
        .op = TEST_DRAW_CIRCLE_FILL,
        .a = x,
        .b = y,
        .e = radius,
        .color = TestDraw_ChargerColor(),
    });
}
