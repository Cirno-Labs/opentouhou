#include "th01/wait.h"

#include "th01/video.h"

#include "core/base/assert.h"

bool Wait_Delay(WAIT *const wait, const uint16_t frames)
{
    if (VSyncIrq_DelayReady(frames)) {
        return true;
    }
    wait->kind = WAIT_DELAY;
    wait->frames = frames;
    return false;
}

bool Wait_Scroll(WAIT *const wait, const int16_t line)
{
    wait->kind = WAIT_SCROLL;
    wait->scroll = line;
    VSyncIrq_BeginWait(&wait->edge);
    return false;
}

bool Wait_Ticks(WAIT *const wait, const uint64_t now, const uint8_t ticks)
{
    wait->kind = WAIT_TICKS;
    wait->since = now;
    wait->ticks = ticks;
    if (now - wait->since < ticks) {
        return false;
    }
    wait->kind = WAIT_IDLE;
    return true;
}

bool Wait_Ready(WAIT *const wait, const uint64_t now)
{
    switch (wait->kind) {
    case WAIT_IDLE:
        return true;
    case WAIT_DELAY:
        if (!VSyncIrq_DelayReady(wait->frames)) {
            return false;
        }
        break;
    case WAIT_SCROLL:
        if (!VSyncIrq_WaitReady(&wait->edge)) {
            return false;
        }
        Video_SetScroll(wait->scroll);
        break;
    case WAIT_TICKS:
        ASSERT(now >= wait->since);
        if (now - wait->since < wait->ticks) {
            return false;
        }
        break;
    }
    wait->kind = WAIT_IDLE;
    return true;
}

bool Wait_Idle(const WAIT *const wait)
{
    return wait->kind == WAIT_IDLE;
}
