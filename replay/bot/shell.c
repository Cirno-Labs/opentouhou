// Gives the clock of shell/game to a recorder, which links shell/null.h
// for the rest.

#include "shell/game.h"

static int32_t m_TickRateCalls;

// Reports full speed every half second of ticks, so that the saved replay
// shows no slowdown.
bool Shell_GetTickRate(float *const out)
{
    m_TickRateCalls++;
    if (m_TickRateCalls < SHELL_TICK_RATE / 2) {
        return false;
    }
    m_TickRateCalls = 0;
    *out = (float)SHELL_TICK_RATE;
    return true;
}

uint64_t Shell_GetWallClock(void)
{
    return 0;
}
