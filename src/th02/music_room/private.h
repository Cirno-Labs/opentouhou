#pragma once

// Shared between logic.c and render.c; nothing outside the music_room/
// directory sees these.

#include "th02/music_room/logic.h"
#include "th02/music_room/polygons.h"
#include "th02/music_room/render.h"

typedef struct {
    uint8_t blue[MUSIC_ROOM_BLUE_SIZE];
    uint8_t comment[4][MUSIC_ROOM_COMMENT_BACKGROUND_SIZE];
} MUSIC_ROOM_BUFFERS;

// The renderer's own state: the captured page snapshots and the polygon
// animation that carries over between visits.
typedef struct {
    const MUSIC_ROOM_ASSETS *assets;
    const TITLE_ASSETS *menu;
    MUSIC_ROOM_BUFFERS buffers;
    MUSIC_POLYGONS polygons;
    uint32_t seed;
    uint8_t logical_page;
} MUSIC_ROOM_RENDER_STATE;

// Writable view of render.c's private state, for tests.
extern MUSIC_ROOM_RENDER_STATE *const p_Render;

// The room's own phase, through browsing, a playing comment and exit.
typedef enum {
    MUSIC_ROOM_RELEASE,
    MUSIC_ROOM_CONTROLS,
    MUSIC_ROOM_COMMENT,
    MUSIC_ROOM_EXIT_RELEASE,
    MUSIC_ROOM_DONE,
} MUSIC_ROOM_PHASE;

// logic.c's own state: the selected and playing track, and the input
// phase.
typedef struct {
    uint8_t playing;
    bool midi_possible;
    bool midi_active;
    MUSIC_ROOM_AUDIO audio;
    void *audio_context;
    MUSIC_ROOM_PHASE phase;
    uint16_t pending_keys;
    uint8_t selection;
    uint8_t next_visible_page;
} MUSIC_ROOM;

// Writable view of logic.c's private state, for tests.
extern MUSIC_ROOM *const p_Room;
