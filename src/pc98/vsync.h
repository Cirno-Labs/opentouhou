#pragma once

#include "core/base/types.h"

typedef struct {
    uint32_t phase;
} VSYNC_CLOCK;

// Advances one 60 Hz host tick and returns whether a 24 kHz-mode VSync
// (56.42 Hz) falls due. A zero clock first fires on tick two; keep it
// across scenes.
bool VSync_Advance(VSYNC_CLOCK *clock);
