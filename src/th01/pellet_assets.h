#pragma once

#include "th01/reiiden_image.h"
#include "th01/video.h"

#define PELLET_EXECUTABLE_SIZE REIIDEN_IMAGE_SIZE

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

// Draws color 7 masked pellets with the source's eight pre-shifted images.
// Uses the drawing page. Native gameplay renders only at 0 < left < 632
// and 64 <= top < 400. Preserves linear row overflow and the source's
// signed row-end test; hidden planar bytes are discarded. Requires image
// 0 through 2.
void PelletAssets_Put(
    const PELLET_ASSETS *assets, int16_t left, int16_t top, int16_t image);

// Draws or restores 16 by 16 delay clouds, quantizing X down to eight
// pixels. Restore first clears the mask on the drawing page, then ORs
// masked page 1 pixels onto page 0. Selects page 0 for nonempty rows only.
// Wraps GRCG writes at 32768 bytes; hidden bytes are discarded. Raw
// restore above that boundary aliases red and green only; the second
// byte at 65536 aliases green only. Ignores non-VRAM writes. Requires
// image 0 or 1.
void PelletAssets_Cloud(
    const PELLET_ASSETS *assets, int16_t left, int16_t top, int16_t image,
    bool restore);
