#pragma once

#include "core/base/types.h"

typedef struct {
    uint32_t phase;
} FRAME_CLOCK;

// Advances one 60 Hz host tick and returns whether a reference VSync is due.
// Uses the 24 kHz mode's 56.423134648 Hz cadence. Zero initialization starts
// at the beginning of a period, so the first VSync is due on host tick two.
// Each VSync is delayed by less than one host tick; the integer phase keeps
// the long-term rate exact. Keep this clock across scene changes, and age
// gameplay counters only where the original gameplay loop advances them.
bool Frame_Advance(FRAME_CLOCK *clock);
