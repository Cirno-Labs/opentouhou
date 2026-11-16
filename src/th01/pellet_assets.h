#pragma once

#include "core/io/reader.h"
#include "th01/video.h"

#define PELLET_EXECUTABLE_SIZE 238612

typedef struct {
    uint8_t pellets[3][8][8][2];
    uint8_t clouds[2][16][2];
} PELLET_ASSETS;

// Loads the embedded masks from the supported REIIDEN.EXE. Checks the full
// executable SHA-256 and MZ header before reading the two tables. scratch
// must hold PELLET_EXECUTABLE_SIZE bytes and may be released after loading.
// Keeps reader open. Allocates only the result from the caller's grouped
// asset allocator, after validation; leaves out null on failure.
RESULT PelletAssets_Load(
    READER *reader, uint8_t *scratch, size_t scratch_size,
    const ALLOCATOR *allocator, PELLET_ASSETS **out);

// Draws white masked pellets with the source's eight pre-shifted images.
// Uses the drawing page. Native gameplay renders only at 0 < left < 632
// and 64 <= top < 400. Preserves linear row overflow and the source's
// signed row-end test; hidden planar bytes are discarded.
void PelletAssets_Put(
    VIDEO *video, const PELLET_ASSETS *assets, int16_t left, int16_t top,
    int16_t image);

// Draws or restores 16 by 16 delay clouds, quantizing X down to eight
// pixels. Restore selects page 0 and copies masked pixels from page 1.
// Wraps GRCG writes at 32768 bytes; hidden bytes are discarded. Raw
// restore above that boundary aliases the red and green planes only,
// leaving blue and intensity cleared. Does not model non-VRAM writes.
void PelletAssets_Cloud(
    VIDEO *video, const PELLET_ASSETS *assets, int16_t left, int16_t top,
    int16_t image, bool restore);
