#pragma once

#include "th01/hud.h"
#include "th01/pellets.h"

// Zeroes the extend threshold index and the resident life count.
void Extend_Init(void);

// Resets the next threshold to 1, as a fresh game starts.
void Extend_Reset(void);

// Reconstructs the next threshold from the signed resident score bits at
// main-scene restart: signed division by 400000 plus one, wrapped to 16 bits.
int16_t Extend_NextForScore(uint32_t resident_score);

// Sets the next threshold from Extend_NextForScore(resident_score).
void Extend_ResetNext(uint32_t resident_score);

// Returns the next extend threshold.
int16_t Extend_Next(void);

// Sets the life count that survives a Continue or a program switch.
void Extend_SyncResidentLives(int32_t lives);

// Returns the life count that survives a Continue or a program switch.
int32_t Extend_ResidentLives(void);

// Advances *next to its next threshold and reports whether score reached the
// wrapped 32-bit product of the signed threshold index and 400000.
bool Extend_Step(int16_t *next, uint32_t score);

// Calls Extend_Step on the next threshold against the live score. On award,
// below the six-life cap, adds a life, draws it, plays effect 15 and raises
// the pellet speed by one raw unit.
void Extend_UpdateAndRender(void);
