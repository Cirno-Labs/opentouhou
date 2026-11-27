#pragma once

#include "core/base/types.h"

typedef struct {
    int32_t score;
    int32_t lives;
    int32_t bombs;
    uint32_t power;
    uint32_t rng;
    uint32_t rng_cursor;
    int32_t player_x;
    int32_t player_y;
} TH02_GAMEPLAY_SAMPLE;

// Reads the gameplay state the harness compares. Kept apart from the
// harness headers, whose SHOTS clashes with th02's.
void Th02_SampleGameplay(TH02_GAMEPLAY_SAMPLE *out);

// Returns how many tile sources the current stage loaded.
uint32_t Th02_LoadedTileSources(void);
