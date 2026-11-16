#pragma once

#include <stdint.h>

// Prefixes the path under which the runner keeps a replay for playback, so
// a resave never names the input file.
#define RUNNER_SCRATCH_PREFIX "scratch/"

// Returns the stage that replay slot (1 to 7) plays: slot 7 is stage 7 for
// Extra and stage 8 for Phantasm (difficulty 5).
int32_t RunnerReplay_GetStage(int32_t slot, int32_t difficulty);
