#pragma once

#include "core/base/types.h"

// Times the PC-98's 24 kHz-mode VSync against the host's 60 Hz tick
// rate, since the two never line up evenly.

typedef struct {
    uint32_t phase;
} VSYNC_CLOCK;

// Advances one 60 Hz host tick and returns whether a 24 kHz-mode VSync
// (56.42 Hz) falls due. A zero clock first fires on tick two; keep it
// across scenes.
bool VSync_Advance(VSYNC_CLOCK *clock);
