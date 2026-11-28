#pragma once

#include "th01/items.h"

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
    int16_t left;
    int16_t top;
    int16_t reserved;
    int16_t velocity_y;
    uint8_t flag;
    uint8_t radius_or_time;
} ITEM;

typedef struct {
    ITEM bombs[ITEMS_BOMB_COUNT];
    ITEM points[ITEMS_POINT_COUNT];
} ITEMS;

// Read-only view of the bomb/point item pools.
extern const ITEMS *const g_Items;

// Writable view of the bomb/point item pools for tests.
extern ITEMS *const p_Items;
