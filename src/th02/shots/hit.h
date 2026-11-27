#pragma once

#include "core/base/types.h"

typedef struct {
    uint32_t frame;
    uint8_t back;
    uint8_t bomb_frame_mask;
} SHOT_HIT_INPUT;

// Returns the shot damage inside the box for level 0..9, back 0..1; as in
// retail, edges are strict. The caller adds the damage to score.delta's
// low word, without carry.
int16_t Shots_Hit(
    SHOT_HIT_INPUT input, int16_t left, int16_t top, int16_t width,
    int16_t height);
