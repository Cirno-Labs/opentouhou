#pragma once

#include "th01/stage.h"

#define ITEMS_BOMB_COUNT 4
#define ITEMS_POINT_COUNT 10

typedef enum {
    ITEM_FREE = 0,
    ITEM_SPLASH = 1,
    ITEM_FALL = 2,
    ITEM_BOUNCE = 3,
    ITEM_COLLECTED = 99,
    ITEM_COLLECTED_OVER_CAP = 100,
} ITEM_FLAG;

typedef struct {
    int16_t left, top, reserved, velocity_y;
    uint8_t flag, radius_or_time;
} ITEM;

// Zero-initialize once; reset preserves every field except flags.
typedef struct {
    ITEM bombs[ITEMS_BOMB_COUNT], points[ITEMS_POINT_COUNT];
} ITEMS;

extern ITEMS g_Items;

// Caches all popup characters from rom in caller-owned asset storage before
// ticks.
RESULT Items_LoadFont(
    const FONT_ROM *rom, const ALLOCATOR *allocator, GAME_TEXT_FONT *out);

// Validates immutable sprites and every required glyph before updates.
RESULT Items_ValidateAssets(const PTN *sprites, const GAME_TEXT_FONT *font);

// Adds at the first free slot using current card coordinates, without RNG.
// A full pool ignores the request.
void Items_Add(int16_t left, int16_t top, bool bomb);

// Clears flags only. Does not erase or reset the shared point value.
void Items_Reset(void);

// Draws visible falling/bouncing sprites, bombs before points.
void Items_Render(const PTN *sprites);

// Runs after the player update: all bomb slots, then all point slots.
// Requires validated assets.
// Keeps the point value live for existing popups, including resets on a
// later drop. Zero formats as five spaces. Collision precedes motion, accepts
// only falling/bouncing items, and has no upper Y bound. Collection moves the
// popup immediately. Updates allocate nothing and read no files, font handles
// or devices.
void Items_UpdateAndRender(const PTN *sprites, const GAME_TEXT_FONT *font);
