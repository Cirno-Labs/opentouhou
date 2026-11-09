#include "th06/timer.h"

void Timer_Init(TIMER *const timer)
{
    *timer = (TIMER) { .previous = -1, .current = 0 };
}

void Timer_Set(TIMER *const timer, const int32_t frames)
{
    *timer = (TIMER) { .previous = -999, .current = frames };
}

void Timer_Tick(TIMER *const timer)
{
    timer->previous = timer->current;
    timer->current++;
}

bool Timer_HasTicked(const TIMER *const timer)
{
    return timer->current != timer->previous;
}
