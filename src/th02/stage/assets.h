#pragma once

#include "th02/background/assets.h"
#include "th02/patterns.h"
#include "th02/stage/data.h"

#include "core/vfs.h"

typedef struct {
    ALLOCATOR allocator;
    BACKGROUND_ASSETS background;
    STAGE_DATA data;
    GFX_COLOR sprite_palette[16];
    uint16_t pattern_first;
    uint16_t pattern_count;
    uint8_t stage;
    bool has_sprite_palette;
} STAGE_ASSETS;

// Loads the stage's tiles, map and sprites after startup slots 0..127; the
// allocator must free individually. Failure leaves patterns unchanged.
RESULT StageAssets_Load(
    VFS *files, uint8_t stage, const ALLOCATOR *allocator,
    PATTERN_BANK *patterns, STAGE_ASSETS *out);

// Frees the assets once nothing borrows them.
void StageAssets_Free(STAGE_ASSETS *assets);
