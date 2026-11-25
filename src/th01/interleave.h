#pragma once

#include "th01/video.h"

#include "core/vfs.h"

#define INTERLEAVE_MASK_COUNT 10

typedef struct {
    uint8_t masks[INTERLEAVE_MASK_COUNT][8];
} INTERLEAVE_ASSETS;

// Loads the ten transition masks from ileave_m.bmp.
RESULT Interleave_LoadAssets(
    VFS *vfs, const ALLOCATOR *allocator, INTERLEAVE_ASSETS **out);

// Combines 8x8 mask mask_id (0..9) into page 0: set bits keep page 0,
// cleared bits take page 1. Rounds left down to eight.
void Interleave_Put8(
    const INTERLEAVE_ASSETS *assets, int16_t left, int16_t top,
    uint8_t mask_id);
