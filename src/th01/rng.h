#pragma once

#include "core/base/types.h"

// Holds the master.lib RNG state of gameplay. The world reset seeds it from
// the session's frame_rand.
extern uint32_t g_Rng;

// Seeds the gameplay RNG.
void Rng_Seed(uint32_t seed);
