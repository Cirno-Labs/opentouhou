#pragma once

#include <stdint.h>

// Measures how many ticks run per second of wall time, over periods of at
// least half a second, as the original games measure their frame rate.
typedef struct {
    uint64_t start_ns;
    int32_t ticks;
    bool started;
} TICK_RATE;

// Adds ticks that run at time now_ns. Returns whether half a second or
// more passed since the period started, and then writes the ticks per
// second of the period into rate and starts the next period. The first
// call starts the first period. Time that goes backwards starts a new
// period.
bool TickRate_Add(
    TICK_RATE *meter, int32_t ticks, uint64_t now_ns, float *rate);
