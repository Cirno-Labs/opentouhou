#pragma once

#include "core/base/result.h"

#include <stdint.h>

// Stands in for a module not yet ported. Each declaration here moves out
// into its own module, under its own name, once that module exists.

// Stands in for the effects quality of the original's configuration, from
// 0 to 2, 2 by default.
extern uint8_t g_EffectsQuality;

RESULT MusicScreen_Register(void);
RESULT Ending_Register(void);

// Stand in for App::AddTotalPlayTime, the restart of its clock, and the
// stop of that clock with App::AddPlayTime.
void PlayTime_Add(void);
void PlayTime_Restart(void);
void PlayTime_Stop(void);

// Stands in for App::AddPlayTime alone, which adds the time since the game
// started to the run time of the play statistics.
void PlayTime_AddRun(void);
