#pragma once

#include <stdint.h>

// Holds the random number generator that th06 and th10 share. A game
// keeps one for gameplay and a second one for randomness outside it.
typedef struct {
    uint16_t seed;
    // Counts the values generated since the seed was set.
    int32_t generated;
} RNG;

void Rng_SetSeed(RNG *rng, uint16_t seed);
// Advances the generator and returns 16 random bits.
uint16_t Rng_U16(RNG *rng);
// Returns 32 random bits: the first Rng_U16 gives the high half.
uint32_t Rng_U32(RNG *rng);
// Returns a float from 0 up to 1.
float Rng_F32(RNG *rng);
float Rng_F32InRange(RNG *rng, float range);
// Returns a value below range. A range of 0 returns 0 and does not
// advance the generator.
uint16_t Rng_U16InRange(RNG *rng, uint16_t range);
uint32_t Rng_U32InRange(RNG *rng, uint32_t range);
