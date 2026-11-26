#pragma once

// Shared state of the music room's renderer: the captured page snapshots
// and the polygon animation that carries over between visits.

#include "th02/music_room/polygons.h"
#include "th02/music_room/render.h"

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
