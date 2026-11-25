#include "th01/vsync_irq.h"

#include <string.h>

VSYNC_COUNTERS g_VSync;

static int16_t M_Increment(const int16_t value)
{
    const uint16_t bits = (uint16_t)((uint16_t)value + 1u);
    return (int16_t)(bits < 0x8000 ? bits : (int32_t)bits - 0x10000);
}

void VSyncIrq_Reset(void)
{
    memset(&g_VSync, 0, sizeof(g_VSync));
}

void VSyncIrq_Edge(void)
{
    g_VSync.count1 = M_Increment(g_VSync.count1);
    g_VSync.count2 = M_Increment(g_VSync.count2);
    g_VSync.edge++;
}

void VSyncIrq_PollEdge(void)
{
    g_VSync.edge++;
}

void VSyncIrq_SwitchProgram(void)
{
    g_VSync.count1 = 1;
    g_VSync.count2 = 1;
}

void VSyncIrq_BeginWait(VSYNC_WAIT *const wait)
{
    wait->edge = g_VSync.edge;
}

bool VSyncIrq_WaitReady(const VSYNC_WAIT *const wait)
{
    return wait->edge != g_VSync.edge;
}

bool VSyncIrq_DelayReady(const uint16_t frames)
{
    if ((uint16_t)g_VSync.count1 < frames) {
        return false;
    }
    g_VSync.count1 = 0;
    return true;
}
