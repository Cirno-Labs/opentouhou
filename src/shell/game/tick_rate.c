#include "tick_rate.h"

#define PERIOD_NS 500000000ULL
#define NS_PER_SECOND 1000000000.0

bool TickRate_Add(
    TICK_RATE *const meter, const int32_t ticks, const uint64_t now_ns,
    float *const rate)
{
    if (!meter->started || now_ns < meter->start_ns) {
        *meter = (TICK_RATE) { .start_ns = now_ns, .started = true };
    }
    meter->ticks += ticks;
    const uint64_t elapsed = now_ns - meter->start_ns;
    if (elapsed < PERIOD_NS) {
        return false;
    }
    *rate = (float)(meter->ticks / ((double)elapsed / NS_PER_SECOND));
    meter->start_ns = now_ns;
    meter->ticks = 0;
    return true;
}
