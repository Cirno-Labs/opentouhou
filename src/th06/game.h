#pragma once

#include "core/base/result.h"
#include "th06/buttons.h"
#include "th06/chain.h"

extern CHAIN g_Chain;
extern BUTTONS g_Buttons;
// Set by Game_Tick to the input the current tick runs with.
extern TH06_INPUT g_Input;

RESULT Game_Init(void);
// Runs one tick with input: the draw chain, then the update chain.
// Returns false when the game quits.
bool Game_Tick(const TH06_INPUT *input);
void Game_Shutdown(void);
