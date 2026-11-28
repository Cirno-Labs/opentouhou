#pragma once

#include "th01/player.h"

#include "core/vfs.h"

typedef struct {
    uint8_t masks[4][8];
} INVINCIBILITY_ASSETS;

typedef struct {
    int16_t frame[8];
    int16_t left[8];
    int16_t top[8];
} INVINCIBILITY;

// Loads the four masks from the last cels of shape8x8.bmp before simulation;
// leaves out empty on failure.
RESULT InvincibilityAssets_Load(VFS *vfs, INVINCIBILITY_ASSETS *out);

// Draws red mask image 0..3 on page 0; a negative left draws nothing.
void Invincibility_Put(
    const INVINCIBILITY_ASSETS *assets, int16_t left, int16_t top,
    int16_t image);

// Draws the invincibility sprites, restoring them when invincibility ends.
// Zero-initialize once per gameplay reset.
void Invincibility_UpdateAndRender(
    INVINCIBILITY *state, const INVINCIBILITY_ASSETS *assets);
