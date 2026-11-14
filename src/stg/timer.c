#include "stg/timer.h"

#define M_SLOW_SPEED_MAX 0.99f

void Timer_Init(TIMER *const timer)
{
    *timer = (TIMER) { .previous = -1, .current = 0, .sub_frame = 0.0f };
}

void Timer_Set(TIMER *const timer, const int32_t frames)
{
    *timer = (TIMER) { .previous = -999, .current = frames, .sub_frame = 0.0f };
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

void Timer_TickAt(TIMER *const timer, const float speed)
{
    timer->previous = timer->current;
    if (speed <= M_SLOW_SPEED_MAX) {
        timer->sub_frame += speed;
        if (timer->sub_frame >= 1.0f) {
            timer->current++;
            timer->sub_frame -= 1.0f;
        }
    } else {
        timer->current++;
    }
}

void Timer_AddAt(TIMER *const timer, const int32_t frames, const float speed)
{
    if (speed > M_SLOW_SPEED_MAX) {
        timer->current += frames;
    } else if (frames < 0) {
        Timer_SubAt(timer, -frames, speed);
    } else {
        timer->previous = timer->current;
        timer->sub_frame += speed * frames;
        while (timer->sub_frame >= 1.0f) {
            timer->current++;
            timer->sub_frame -= 1.0f;
        }
    }
}

void Timer_SubAt(TIMER *const timer, const int32_t frames, const float speed)
{
    if (speed > M_SLOW_SPEED_MAX) {
        timer->current -= frames;
    } else if (frames < 0) {
        Timer_AddAt(timer, -frames, speed);
    } else {
        timer->previous = timer->current;
        timer->sub_frame -= speed * frames;
        while (timer->sub_frame < 0.0f) {
            timer->current--;
            timer->sub_frame += 1.0f;
        }
    }
}

float Timer_AsFloat(const TIMER *const timer)
{
    return (float)timer->current + timer->sub_frame;
}
