#pragma once

// Records the draws that the fake draw modules take instead of drawing, so
// a test linked with them asserts what was drawn, not pixels.

#include <stdbool.h>
#include <stdint.h>

#define TEST_DRAW_CALLS_MAX 512

typedef enum {
    TEST_DRAW_DOT, // a=left, b=top, e=size
    TEST_DRAW_CIRCLE, // a=x, b=y, e=radius
    TEST_DRAW_CIRCLE_FILL, // a=x, b=y, e=radius
    TEST_DRAW_BOX_FILL, // a=left, b=top, c=right, d=bottom
    TEST_DRAW_HLINE, // a=left, c=right, b=y
    TEST_DRAW_VLINE, // a=x, b=top, d=bottom
    TEST_DRAW_LINE, // a=left, b=top, c=right, d=bottom
    TEST_DRAW_ROUND_BOX_FILL, // a=left, b=top, c=right, d=bottom, e=radius
    TEST_DRAW_POLYGON_FILL, // a=count, pattern=vertices
    TEST_DRAW_SPRITE, // a=left, b=top, pattern
    TEST_DRAW_SPRITE_ROLL, // a=left, b=top, pattern
    TEST_DRAW_SPRITE_RECT, // a=left, b=top, pattern
    TEST_DRAW_SPRITE_ROLL_TINY, // a=left, b=top, pattern
    TEST_DRAW_SPRITE_PLANE, // a=left, b=top, plane, e=mode, pattern
    TEST_DRAW_SPRITE_ROLL_PLANE, // a=left, b=top, plane, e=mode, pattern
    TEST_DRAW_SPRITE_ZOOM, // a=left, b=top, c=scale, pattern
    TEST_DRAW_SPRITE_OPAQUE, // a=left, b=top, pattern
    TEST_DRAW_MASK_BYTE, // a=offset, color, flag=mask (low byte of e)
    TEST_DRAW_GAIJI, // a=left, b=top, c=glyph, color, flag=carry
    TEST_DRAW_GAIJI_STRING, // a=left, b=top, c=step, color, pattern=glyphs
    TEST_DRAW_TEXT, // a=left, b=top, c=count, e=col_and_fx, pattern=glyphs
} TEST_DRAW_OP;

typedef struct {
    TEST_DRAW_OP op;
    int32_t a;
    int32_t b;
    int32_t c;
    int32_t d;
    uint32_t e;
    uint8_t color;
    uint8_t plane;
    bool flag;
    const void *pattern;
} TEST_DRAW_CALL;

typedef struct {
    TEST_DRAW_CALL calls[TEST_DRAW_CALLS_MAX];
    int32_t count;
} TEST_DRAW_LOG;

// The calls recorded since the last TestDraw_Reset.
extern TEST_DRAW_LOG g_TestDraw;

// Forgets the recorded calls.
void TestDraw_Reset(void);

// Appends call to the log, dropping it past TEST_DRAW_CALLS_MAX.
void TestDraw_Record(TEST_DRAW_CALL call);

// The color the charger paints, one bit per plane.
uint8_t TestDraw_ChargerColor(void);
