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

typedef struct {
    PLAYER *player;
    STAGE_GAMEPLAY *gameplay;
    uint16_t *point_value;
    VIDEO *video;
    const PTN *sprites;
    const GAME_TEXT_FONT *font;
    void (*hud_bomb)(void *user, int16_t previous_slot);
    void (*hud_score_and_combo)(void *user);
    void (*effect)(void *user, int16_t number);
    void *user;
} ITEM_CONTEXT;

// Caches all popup characters in caller-owned asset storage before ticks.
// Uses the portable font substitute described by GameText_Load.
RESULT Items_LoadFont(
    FONT *font, const ALLOCATOR *allocator, GAME_TEXT_FONT *out);

// Validates immutable sprites and every required glyph before updates.
RESULT Items_ValidateAssets(const PTN *sprites, const GAME_TEXT_FONT *font);

// Adds at the first free slot using current card coordinates, without RNG.
// A full pool ignores the request. Coordinates can be borrowed from the
// Stage item_add callback; no stage pointer is retained.
void Items_Add(ITEMS *items, int16_t left, int16_t top, bool bomb);

// Clears flags only. Does not erase or reset the shared point value.
void Items_Reset(ITEMS *items);

// Draws visible falling/bouncing sprites, bombs before points.
void Items_Render(const ITEMS *items, VIDEO *video, const PTN *sprites);

// Runs after the player update: all bomb slots, then all point slots.
// Requires validated assets and all synchronous callbacks. HUD observes
// updated shared fields; callbacks must not mutate items or shared state.
// Keeps the point value live for existing popups, including resets on a
// later drop. Zero formats as five spaces. Collision precedes motion, accepts
// only falling/bouncing items, and has no upper Y bound. Collection moves the
// popup immediately. Updates allocate nothing and read no files, font handles
// or devices. The gameplay scene must supply real HUD/effect consumers and call
// order.
void Items_UpdateAndRender(ITEMS *items, const ITEM_CONTEXT *context);
