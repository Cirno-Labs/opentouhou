#pragma once

#include "th02/draw.h"
#include "th02/title.h"

#define MUSIC_ROOM_TRACK_COUNT 15
#define MUSIC_ROOM_QUIT 16
#define MUSIC_ROOM_COMMENT_LINES 20
#define MUSIC_ROOM_BLUE_SIZE 32000
#define MUSIC_ROOM_COMMENT_BACKGROUND_SIZE 12800

typedef struct {
    TITLE_IMAGE background;
    DRAW_TEXT choices[MUSIC_ROOM_QUIT + 1];
    DRAW_TEXT comments[MUSIC_ROOM_TRACK_COUNT][MUSIC_ROOM_COMMENT_LINES];
} MUSIC_ROOM_ASSETS;

// Resets snapshots, animation and seed, once at startup.
void MusicRoomRender_Init(void);

// Draws the music room from 640x400 images; polygons and seed carry over
// from the last visit.
void MusicRoomRender_Begin(
    const MUSIC_ROOM_ASSETS *assets, const TITLE_ASSETS *menu,
    uint8_t selection);

// Redraws the track list on both pages.
void MusicRoomRender_Track(uint8_t selection, uint8_t color);

// Draws the comment of selection over the captured background.
void MusicRoomRender_Comment(uint8_t selection);

// Draws one frame of polygons over the blue-plane snapshot.
void MusicRoomRender_Frame(void);
void MusicRoomRender_Flip(uint8_t drawing_page, uint8_t visible_page);

// Restores both menu pages.
void MusicRoomRender_End(void);
