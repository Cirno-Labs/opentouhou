#pragma once

#include "core/base/result.h"

#include "stg/chain.h"
#include "stg/rng.h"

extern CHAIN g_Chain;
// Draws all of the gameplay randomness.
extern RNG g_Rng;

RESULT Game_Init(void);
// Runs one tick: the draw chain, then the update chain. A reset tears the
// game down and starts it again from boot. Returns false when the game
// quits.
bool Game_Tick(void);
void Game_Shutdown(void);
