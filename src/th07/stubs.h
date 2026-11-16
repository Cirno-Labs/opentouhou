#pragma once

#include "core/base/result.h"

#include <stdint.h>

// Stands in for a module not yet ported. Each declaration here moves out
// into its own module, under its own name, once that module exists.

// Stands in for the effects quality of the original's configuration, from
// 0 to 2, 2 by default.
extern uint8_t g_EffectsQuality;

// Stands in for th07/resultscreen's screen in mode 0 or 1; mode 2 only
// makes the RNG draws of its save of score.dat.
RESULT ResultScreen_Register(int32_t mode);

RESULT MusicScreen_Register(void);
RESULT Ending_Register(void);

// Stand in for th07/resultscreen's records of an attempt at spell card
// index, and of its capture with bonus, for the shot of
// g_GameManager.run.shot_index. Neither runs in replays. The capture adds
// one to g_GameManager.records.spell_captures[index], up to 9999.
void ResultScreen_AddSpellCardAttempt(int32_t index, const char *name);
void ResultScreen_AddSpellCardCapture(int32_t index, int32_t bonus);

// Stands in for th07/resultscreen's record of spell card index for the shot
// of g_GameManager.run.shot_index: its captures and its attempts.
void ResultScreen_GetSpellCardRecord(
    int32_t index, int32_t *captures, int32_t *attempts);

// Stands in for the reads of score.dat that a new game makes into
// g_GameManager: the high score and its continues, the spell cards, the
// clears and the practice scores.
RESULT ResultScreen_LoadGameRecords(void);

// Stands in for the reads of score.dat that the title makes into
// g_GameManager.records: LoadClears, LoadPracticeScores, LoadSpellCards and
// the play time.
void ResultScreen_LoadTitleRecords(void);

// Stands in for th07/replay's save of the replay being played again, when
// the score differs from the one the replay holds.
void Replay_ResaveIfScoreChanged(void);

// Stand in for App::AddTotalPlayTime, the restart of its clock, and the
// stop of that clock with App::AddPlayTime.
void PlayTime_Add(void);
void PlayTime_Restart(void);
void PlayTime_Stop(void);
