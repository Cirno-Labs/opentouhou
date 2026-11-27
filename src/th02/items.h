#pragma once

#include "th02/entity.h"
#include "th02/hud.h"
#include "th02/performance.h"
#include "th02/point_numbers.h"
#include "th02/random.h"

#define ITEM_COUNT 20

typedef enum {
    ITEM_POWER,
    ITEM_POINT,
    ITEM_BOMB,
    ITEM_BIG_POWER,
    ITEM_LIFE,
} ITEM_KIND;

typedef struct {
    uint8_t state;
    uint8_t kind;
    ENTITY_POINT position[2];
    int16_t speed_y;
    int16_t drift_x;
    int16_t age;
} ITEM;

typedef struct {
    ITEM slots[ITEM_COUNT];
    uint16_t big_power;
    int16_t point_count;
    int16_t frame;
    int16_t overflow;
    uint8_t ring_cursor;
    uint8_t cycle;
    int32_t frame_score;
    ENTITY_POINT current;
} ITEMS;

extern const ITEMS *const g_Items;

// Clears the item state, once per MAIN process.
void Items_Init(void);

// Frees every slot and resets the point numbers at stage start, keeping
// payloads, drop cycles, overflow and performance.
void Items_ResetStage(uint8_t rank);

// Adds an item; a full bank still uses up big-power overrides, and at five
// lives a life item becomes a bomb.
void Items_Add(int16_t left, int16_t top, int16_t kind);

// Adds a pending big power, else on every third call a ring item or, rarely,
// a bomb; the ring advances even when the bank is full.
void Items_AddRandom(int16_t left, int16_t top);

// Sets big_power from the stage number, for a session start or continue.
void Items_BeginSession(uint8_t stage);

// Resets overflow for a continue.
void Items_ResetOverflow(void);

// Drops up to five items after a miss into free slots, spreading them by
// player.current.x, not left.
void Items_Drop(int16_t left, int16_t top);

// Invalidates the tiles under the items, frees removed slots and
// invalidates the point numbers.
void Items_Invalidate(PLAYFIELD_PAGES pages);

// Moves, collects and draws the items; requires overflow 0..42.
void Items_Update(
    PLAYFIELD_PAGES pages, uint8_t rank, const PATTERN_BANK *patterns,
    const POINT_NUMBER_FONT *font);
