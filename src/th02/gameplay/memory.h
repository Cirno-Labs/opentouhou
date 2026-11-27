#pragma once

#include "th02/bomb/assets.h"
#include "th02/gameplay/assets.h"
#include "th02/stage/assets.h"

// Starts tracking retail's 288000-byte gameplay working set.
void GameplayMemory_Begin(
    const GAMEPLAY_ASSETS *assets, const BOMB_ASSETS *bomb, bool demo);
void GameplayMemory_LoadStage(
    const STAGE_ASSETS *stage, const PATTERN_BANK *patterns);
void GameplayMemory_SelectPatterns(const PATTERN_BANK *patterns);
void GameplayMemory_Shutdown(void);

// Copies the other page to destination through 32000 scratch bytes of the
// working set; fails, changing nothing, when they are not free.
RESULT GameplayMemory_CopyPage(uint8_t destination);
