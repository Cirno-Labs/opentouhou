#pragma once

#include "th06/screens/result/types.h"

#include "core/base/result.h"

#include <stdint.h>

// Stands in for a module not yet ported. Each function here moves out
// into its own module, under its own name, once that module exists.

RESULT TitleScene_SetUp(int32_t mode);
RESULT ResultScreen_Register(int32_t mode);
RESULT MusicRoom_Register(void);
RESULT Ending_Register(void);
void Replay_Save(void);
void ResultScreen_AddSpellCardAttempt(
    int32_t index, const char *name, int32_t bonus);
void ResultScreen_AddSpellCardCapture(int32_t index, int32_t shot_type);
RESULT Replay_Register(int32_t mode, const char *path);
void Replay_EndStage(void);
void ResultScreen_LoadGameRecords(
    uint32_t *high_score, SPELL_CARD_RECORD *spell_cards, CLEAR_RECORD *clears,
    PRACTICE_RECORD *practice, int32_t shot, int32_t difficulty);
void AsciiText_Initialize(void);
