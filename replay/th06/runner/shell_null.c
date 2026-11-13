#include "shell/game.h"

#include <stdint.h>

static int32_t m_TickRateCalls;

bool Shell_GetPadPress(INPUT_BINDING *const out)
{
    return false;
}

size_t Shell_GetBindings(
    const size_t action, INPUT_BINDING *const out, const size_t capacity)
{
    return 0;
}

void Shell_RequestBind(const size_t action, const INPUT_BINDING binding)
{
}

void Shell_RequestUnbind(const size_t action, const INPUT_BINDING binding)
{
}

void Shell_RequestResetPadBindings(void)
{
}

// Reports full speed every half second of ticks, so that the result
// screen and saved replays show no slowdown.
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

bool Shell_IsFullscreen(void)
{
    return false;
}

void Shell_RequestFullscreen(const bool fullscreen)
{
}
