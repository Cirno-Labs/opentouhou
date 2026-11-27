#pragma once

#include "th02/tram.h"

typedef struct {
    TRAM_TEXT labels[10];
    TRAM_TEXT point;
    TRAM_TEXT times;
    TRAM_TEXT equals;
    ALLOCATOR allocator;
} STAGE_BONUS_ASSETS;

// Loads the bonus labels; the allocator must free individually.
RESULT StageBonusAssets_Load(
    const ALLOCATOR *allocator, STAGE_BONUS_ASSETS *out);

// Frees the labels; accepts an empty cache.
void StageBonusAssets_Free(STAGE_BONUS_ASSETS *assets);
