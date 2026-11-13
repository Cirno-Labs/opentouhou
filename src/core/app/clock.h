// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <stdint.h>

// Converts wall-clock time into a whole number of fixed-length ticks.
// Time is kept in nanoseconds multiplied by the tick rate, so a rate that
// does not divide one second, such as 60 Hz, accumulates no rounding error.
typedef struct {
    uint64_t rate;
    int max_ticks;
    uint64_t last_ns;
    uint64_t pending;
    bool started;
} CLOCK;

// Prepares a clock for rate ticks per second. One call to Clock_Advance
// returns at most max_ticks; the time beyond that is dropped, so that the
// game slows down instead of trying to catch up without end.
void Clock_Init(CLOCK *clock, int rate, int max_ticks);

// Returns how many ticks are due at time now_ns. The first call starts the
// clock and returns 0.
int Clock_Advance(CLOCK *clock, uint64_t now_ns);

// Returns the time in nanoseconds until the next tick is due.
uint64_t Clock_NsUntilNextTick(const CLOCK *clock);
