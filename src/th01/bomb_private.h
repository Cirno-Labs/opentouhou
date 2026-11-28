#pragma once

#include "th01/bomb.h"

typedef struct {
    PALETTE white;
    PALETTE bright;
    int16_t left[18];
    int16_t top[18];
    int16_t frame;
    int16_t scroll;
    int8_t tint_offset;
    VSYNC_WAIT wait;
    bool waiting;
} BOMB;

// Writable view of the bomb animation for tests.
extern BOMB *const p_Bomb;
