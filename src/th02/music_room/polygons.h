#pragma once

// The music room's background polygons: sixteen shapes drifting and
// spinning down the screen, replayed from a shared RNG seed.

#include "pc98/zun/math16.h"

#include "core/base/types.h"

#define MUSIC_POLYGON_COUNT 16

typedef struct {
    int16_t x;
    int16_t y_subpixel;
    int16_t velocity_x;
    int16_t velocity_y_subpixel;
    uint8_t angle;
    int8_t angular_velocity;
} MUSIC_POLYGON;

// Keeps polygon motion across visits, with pixel X, Q12.4 Y and velocity.
typedef struct {
    MUSIC_POLYGON polygons[MUSIC_POLYGON_COUNT];
    bool initialized;
} MUSIC_POLYGONS;

// Draws a closed polygon of count 3..6 sides; vertices holds count+1
// points, the last repeating the first, valid only during the call.
typedef void (*MUSIC_POLYGON_DRAW)(
    void *context, const MATH16_VECTOR *vertices, uint8_t count);

// Moves the polygons and draws their previous shapes, using the RNG in
// retail order even with null drawing.
void MusicPolygons_UpdateAndRender(
    MUSIC_POLYGONS *state, uint32_t *seed, MUSIC_POLYGON_DRAW draw,
    void *context);
