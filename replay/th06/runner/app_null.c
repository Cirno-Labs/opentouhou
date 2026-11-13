#include "core/app.h"

#include <stdint.h>

static int32_t m_TickRateCalls;

bool App_GetPadPress(INPUT_BINDING *const out)
{
    return false;
}

size_t App_GetBindings(
    const size_t action, INPUT_BINDING *const out, const size_t capacity)
{
    return 0;
}

void App_RequestBind(const size_t action, const INPUT_BINDING binding)
{
}

void App_RequestUnbind(const size_t action, const INPUT_BINDING binding)
{
}

void App_RequestResetPadBindings(void)
{
}

// Reports full speed every half second of ticks, so that the result
// screen and saved replays show no slowdown.
bool App_GetTickRate(float *const out)
{
    m_TickRateCalls++;
    if (m_TickRateCalls < APP_TICK_RATE / 2) {
        return false;
    }
    m_TickRateCalls = 0;
    *out = (float)APP_TICK_RATE;
    return true;
}

bool App_IsFullscreen(void)
{
    return false;
}

void App_RequestFullscreen(const bool fullscreen)
{
}
