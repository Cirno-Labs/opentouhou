#pragma once

#include "core/base/result.h"

#include <stdint.h>

// Stands in for a module not yet ported. Each function here moves out
// into its own module, under its own name, once that module exists.

RESULT TitleScene_SetUp(int32_t mode);
RESULT GameManager_Register(void);
void GameManager_Remove(void);
RESULT ResultScreen_Register(int32_t mode);
RESULT MusicRoom_Register(void);
RESULT Ending_Register(void);
void Replay_Save(void);
