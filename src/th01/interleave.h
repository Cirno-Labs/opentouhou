#pragma once

#include "th01/reiiden_image.h"
#include "th01/video.h"

#define INTERLEAVE_MASK_COUNT 10

typedef struct {
    uint8_t masks[INTERLEAVE_MASK_COUNT][8];
} INTERLEAVE_ASSETS;

// Loads the ten transition masks from the supported REIIDEN.EXE. Keeps the
// reader open and uses caller scratch of REIIDEN_IMAGE_SIZE bytes only while
// loading. Allocates the immutable result from the grouped asset allocator;
// leaves out null on error. Does not copy executable storage into the result.
RESULT Interleave_LoadAssets(
    READER *reader, uint8_t *scratch, size_t scratch_size,
    const ALLOCATOR *allocator, INTERLEAVE_ASSETS **out);

// Combines an 8x8 tile into page 0: set mask bits retain page 0, cleared bits
// take page 1. Rounds left down to eight pixels. Requires left 0..639, top
// 0..392 and mask_id 0..9; defines a visible-tile domain instead of arbitrary
// raw pointer accesses. Selects drawing page 0 and preserves display state,
// page 1 and palette. Allocates nothing and performs no IO.
void Interleave_Put8(
    VIDEO *video, const INTERLEAVE_ASSETS *assets, int16_t left, int16_t top,
    uint8_t mask_id);
