#pragma once

#include "core/base/types.h"

typedef enum {
    ENTITY_FREE,
    ENTITY_ACTIVE,
    ENTITY_REMOVE,
} ENTITY_STATE;

typedef struct {
    int16_t x;
    int16_t y;
} ENTITY_POINT;

// The two pages and the scroll line that one gameplay frame draws with.
typedef struct {
    uint8_t back;
    uint8_t front;
    int16_t scroll;
} PLAYFIELD_PAGES;
