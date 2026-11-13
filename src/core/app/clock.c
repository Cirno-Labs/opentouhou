// SPDX-License-Identifier: GPL-3.0-only

#include "clock.h"

#define NS_PER_SECOND 1000000000ULL

void Clock_Init(CLOCK *const clock, const int rate, const int max_ticks)
{
    *clock = (CLOCK) {
        .rate = (uint64_t)rate,
        .max_ticks = max_ticks,
    };
}

int Clock_Advance(CLOCK *const clock, const uint64_t now_ns)
{
    if (!clock->started || now_ns < clock->last_ns) {
        clock->started = true;
        clock->last_ns = now_ns;
        clock->pending = 0;
        return 0;
    }

    clock->pending += (now_ns - clock->last_ns) * clock->rate;
    clock->last_ns = now_ns;

    uint64_t ticks = clock->pending / NS_PER_SECOND;
    clock->pending %= NS_PER_SECOND;
    if (ticks > (uint64_t)clock->max_ticks) {
        ticks = (uint64_t)clock->max_ticks;
    }
    return (int)ticks;
}

uint64_t Clock_NsUntilNextTick(const CLOCK *const clock)
{
    return (NS_PER_SECOND - clock->pending + clock->rate - 1) / clock->rate;
}
