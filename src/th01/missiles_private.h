#pragma once

#include "th01/missiles.h"

#define MISSILES_COUNT 50

typedef enum {
    MISSILE_FREE,
    MISSILE_MOVING,
    MISSILE_HIT,
    MISSILE_HIT_LAST,
} MISSILE_FLAG;

typedef struct {
    int16_t left;
    int16_t top;
    int16_t previous_left;
    int16_t previous_top;
    MATH16_VECTOR velocity;
    int16_t unused;
    int8_t reserved;
    uint8_t flag;
} MISSILE;

typedef struct {
    MISSILE slots[MISSILES_COUNT];
} MISSILES;

// Read-only view of the missile slots.
extern const MISSILES *const g_Missiles;

// Writable view of the missile slots for tests.
extern MISSILES *const p_Missiles;
