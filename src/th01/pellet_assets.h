#pragma once

#include "th01/video.h"

#include "core/vfs.h"

typedef struct {
    uint8_t pellets[3][8][8][2];
    uint8_t clouds[2][16][2];
} PELLET_ASSETS;

// Loads the pellets from pellet.bmp, shifted right by 0 to 7 pixels into
// big-endian 16-bit rows, and the clouds from pellet_c.bmp. Allocates the
// result from the caller's grouped asset allocator; leaves out null on
// failure.
RESULT PelletAssets_Load(
    VFS *vfs, const ALLOCATOR *allocator, PELLET_ASSETS **out);

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
