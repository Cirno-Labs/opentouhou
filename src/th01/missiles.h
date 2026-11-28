#pragma once

#include "th01/player.h"

#include "pc98/zun/math16.h"

// Clears all state for a new gameplay session.
void Missiles_Init(void);

// Validates boss3_m.ptn loaded with Ptn_Load into the caller's asset arena.
// Maps the source PTN bank to local images 0..3 and explosion image 4.
RESULT Missiles_ValidateSprites(const PTN *sprites);

// Adds a missile in the first free slot; returns false when the pool is full.
// Requires finite velocities.
bool Missiles_Add(
    int16_t left, int16_t top, double velocity_x, double velocity_y,
    int8_t reserved);

// Clears flags only, without restoring pixels or clearing history.
void Missiles_ResetStage(void);

// Moves, explodes, draws and hit-tests every missile in pool order.
// frame_rand bit 0 selects the slots that draw and advance explosions.
void Missiles_Update(uint16_t frame_rand, const PTN *sprites);
