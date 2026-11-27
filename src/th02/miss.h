#pragma once

#include "th02/entity.h"
#include "th02/patterns.h"

typedef struct {
    uint8_t frame;
    uint8_t stage_misses;
    uint8_t item_gameover;
    uint8_t active;
} MISS;

extern const MISS *const g_Miss;

void Miss_Init(void);

// Resets the miss state at stage start, keeping the gameover-drop flag.
void Miss_ResetStage(void);

// Clears the gameover-drop flag.
void Miss_ClearGameOver(void);

// Runs the miss animation, refilling start_bombs and keeping weapon
// cadence; leaves player.hit=255 on game over.
void Miss_Update(
    PLAYFIELD_PAGES pages, const PATTERN_BANK *patterns, uint8_t start_bombs);
