#include "th01/frame.h"

#include <string.h>

FRAME_VSYNC g_VSync;

static int16_t M_Increment(const int16_t value)
{
    const uint16_t bits = (uint16_t)((uint16_t)value + 1u);
    return (int16_t)(bits < 0x8000 ? bits : (int32_t)bits - 0x10000);
}

void Frame_Reset(void)
{
    memset(&g_VSync, 0, sizeof(g_VSync));
}

void Frame_VSync(void)
{
    g_VSync.count1 = M_Increment(g_VSync.count1);
    g_VSync.count2 = M_Increment(g_VSync.count2);
    g_VSync.edge++;
}

void Frame_PollVSync(void)
{
    g_VSync.edge++;
}

void Frame_SwitchProgram(void)
{
    g_VSync.count1 = 1;
    g_VSync.count2 = 1;
}

void Frame_BeginWait(FRAME_WAIT *const wait)
{
    wait->edge = g_VSync.edge;
}

bool Frame_WaitReady(const FRAME_WAIT *const wait)
{
    return wait->edge != g_VSync.edge;
}

bool Frame_DelayReady(const uint16_t frames)
{
    if ((uint16_t)g_VSync.count1 < frames) {
        return false;
    }
    g_VSync.count1 = 0;
    return true;
}
