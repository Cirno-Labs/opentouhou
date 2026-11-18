#pragma once

#include "core/base/result.h"
#include "th07/buttons.h"

#include "stg/chain.h"
#include "stg/rng.h"

extern CHAIN g_Chain;
// Draws all of the gameplay randomness.
extern RNG g_Rng;
extern BUTTONS g_Buttons;
// 1 for full speed, less while ECL slows the game.
extern float g_GameSpeed;
// The input used by the update chain.
extern TH07_INPUT g_Input;

RESULT Game_Init(void);

// Runs the 30 update steps before the first draw at boot. Does nothing
// after boot. Returns the update chain's quit or reset request.
CHAIN_RUN Game_Warmup(const TH07_INPUT *input);

// Runs one tick with input: the draw chain, then the update chain. A
// reset tears the game down and starts it again from boot. Returns false
// when the game quits.
bool Game_Tick(const TH07_INPUT *input);
void Game_Shutdown(void);
