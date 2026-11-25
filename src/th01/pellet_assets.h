#pragma once

#include "th01/video.h"

#include "core/vfs.h"

typedef struct {
    uint8_t pellets[3][8][8][2];
    uint8_t clouds[2][16][2];
} PELLET_ASSETS;

// Loads pellet.bmp in eight pre-shifted copies and the clouds of pellet_c.bmp
// from the caller's grouped allocator; leaves out null on failure.
RESULT PelletAssets_Load(
    VFS *vfs, const ALLOCATOR *allocator, PELLET_ASSETS **out);

// Draws pellet image 0..2 in color 7 on the drawing page.
void PelletAssets_Put(
    const PELLET_ASSETS *assets, int16_t left, int16_t top, int16_t image);

// Draws or restores 16 by 16 delay cloud 0 or 1, with X rounded down to 8.
void PelletAssets_Cloud(
    const PELLET_ASSETS *assets, int16_t left, int16_t top, int16_t image,
    bool restore);
