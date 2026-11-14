#pragma once

#include "core/base/result.h"

#include <stdint.h>

// Stands in for a module not yet ported. Each declaration here moves out
// into its own module, under its own name, once that module exists.

// Stands in for the part of th07/game_manager's state that the scene
// switch reads and writes.
typedef struct {
    struct {
        int32_t difficulty; // 0 to 3, 4 for Extra, 5 for Phantasm
        int32_t stage;
    } run;
    struct {
        bool in_practice;
    } mode;
    struct {
        int32_t clears;
    } play_counts[6]; // by difficulty
} GAME_MANAGER;

extern GAME_MANAGER g_GameManager;

RESULT TitleScene_SetUp(int32_t mode);
RESULT GameManager_Register(void);
void GameManager_Remove(void);
RESULT ResultScreen_Register(int32_t mode);
RESULT MusicScreen_Register(void);
RESULT Ending_Register(void);
void Replay_Discard(void);
