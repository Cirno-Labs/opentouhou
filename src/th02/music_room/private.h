#pragma once

// Shared state of the music room's renderer: the captured page snapshots
// and the polygon animation that carries over between visits.

#include "th02/music_room/render.h"

#include "core/base/types.h"
#include "pc98/zun/math16.h"

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

typedef struct {
    uint8_t blue[MUSIC_ROOM_BLUE_SIZE];
    uint8_t comment[4][MUSIC_ROOM_COMMENT_BACKGROUND_SIZE];
} MUSIC_ROOM_BUFFERS;

typedef struct {
    const MUSIC_ROOM_ASSETS *assets;
    const TITLE_ASSETS *menu;
    MUSIC_ROOM_BUFFERS buffers;
    MUSIC_POLYGONS polygons;
    uint32_t seed;
    uint8_t logical_page;
} MUSIC_ROOM_RENDER_STATE;
