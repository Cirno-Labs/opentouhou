#pragma once

#include "th07/anm.h"
#include "th07/player/state.h"

#include "stg/chain.h"

#include <stdint.h>

// Shares the shots of th07/player between its files.

// Holds the update element and the two draw elements of the player.
typedef struct {
    CHAIN_ELEM calc;
    CHAIN_ELEM draw;
    CHAIN_ELEM draw2;
} PLAYER_ELEMS;

// Fires the shots of the power level, unfocused or focused, for frame of the
// burst, each into the next free bullet. A shot that does not fire leaves
// the bullet for the next shot. When the bullets run out, the shots left do
// not fire.
void Player_FireShots(int32_t frame);

// Starts the fire timer from 0 unless it runs.
void Player_StartFiring(void);

// Runs the fire timer, firing on each new frame except during a bomb of
// Marisa B, and stops it after 30 frames or while dying or respawning.
void Player_UpdateFiring(void);

// Drops the lasers whose options are gone, or all of them while dying, runs
// down the laser timers, and moves the bullets through their behaviors.
void Player_UpdateShots(void);

// Draws the bullets in state, in the play area. Flying bullets also draw
// their trails.
void Player_DrawShots(PLAYER_BULLET_STATE state, const ANM_DRAW *draw);
