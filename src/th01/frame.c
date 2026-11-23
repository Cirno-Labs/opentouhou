#include "th01/frame.h"

static int16_t M_Increment(const int16_t value)
{
    const uint16_t bits = (uint16_t)((uint16_t)value + 1u);
    return (int16_t)(bits < 0x8000 ? bits : (int32_t)bits - 0x10000);
}

void Frame_VSync(FRAME_VSYNC *const vsync)
{
    vsync->count1 = M_Increment(vsync->count1);
    vsync->count2 = M_Increment(vsync->count2);
    vsync->edge++;
}

void Frame_PollVSync(FRAME_VSYNC *const vsync)
{
    vsync->edge++;
}

void Frame_SwitchProgram(FRAME_VSYNC *const vsync)
{
    vsync->count1 = 1;
    vsync->count2 = 1;
}

void Frame_BeginWait(FRAME_WAIT *const wait, const FRAME_VSYNC *const vsync)
{
    wait->edge = vsync->edge;
}

bool Frame_WaitReady(
    const FRAME_WAIT *const wait, const FRAME_VSYNC *const vsync)
{
    return wait->edge != vsync->edge;
}

bool Frame_DelayReady(FRAME_VSYNC *const vsync, const uint16_t frames)
{
    if ((uint16_t)vsync->count1 < frames) {
        return false;
    }
    vsync->count1 = 0;
    return true;
}
