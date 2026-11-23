#pragma once

#include "th01/player.h"
#include "th01/reiiden_image.h"

typedef struct {
    uint8_t masks[4][8];
} INVINCIBILITY_ASSETS;

typedef struct {
    int16_t frame[8], left[8], top[8];
} INVINCIBILITY;

// Loads four masks from the verified executable before simulation. Uses
// caller scratch only during loading and leaves out empty on failure.
RESULT Invincibility_LoadAssets(
    READER *reader, uint8_t *scratch, size_t scratch_size,
    INVINCIBILITY_ASSETS *out);

// Draws the source's red mask, including byte spill at the right edge and
// 15-bit planar wrapping. Missing hidden planar bytes are discarded. Left
// below zero or image outside 0..3 draws nothing. Selects drawing page zero.
void Invincibility_Put(
    const INVINCIBILITY_ASSETS *assets, int16_t left, int16_t top,
    int16_t image);

// Zero-initialize once per gameplay reset. Retains sprite positions across
// invincibility periods, uses three RNG draws per new sprite and restores
// all eight old rectangles once when invincibility ends. Calls allocate
// nothing, advance no clocks and read only supplied actor and RNG state.
void Invincibility_UpdateAndRender(
    INVINCIBILITY *state, const INVINCIBILITY_ASSETS *assets,
    const PLAYER *player, uint32_t *rng);
