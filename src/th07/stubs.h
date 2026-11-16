#pragma once

#include "core/base/result.h"

#include <stdint.h>

// Stands in for a module not yet ported. Each declaration here moves out
// into its own module, under its own name, once that module exists.

// Stands in for the effects quality of the original's configuration, from
// 0 to 2, 2 by default.
extern uint8_t g_EffectsQuality;

// Stands in for th07/resultscreen's screen in mode 0 or 1.
RESULT ResultScreen_Register(int32_t mode);

RESULT MusicScreen_Register(void);
RESULT Ending_Register(void);

// Stands in for th07/replay's save of the replay being played again, when
// the score differs from the one the replay holds.
void Replay_ResaveIfScoreChanged(void);

// Stand in for App::AddTotalPlayTime, the restart of its clock, and the
// stop of that clock with App::AddPlayTime.
void PlayTime_Add(void);
void PlayTime_Restart(void);
void PlayTime_Stop(void);
