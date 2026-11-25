#include "th01/vsync_irq.h"

#include <string.h>

static VSYNC_COUNTERS m_VSync;
const VSYNC_COUNTERS *const g_VSync = &m_VSync;

static int16_t M_Increment(const int16_t value)
{
    const uint16_t bits = (uint16_t)((uint16_t)value + 1u);
    return (int16_t)(bits < 0x8000 ? bits : (int32_t)bits - 0x10000);
}

void VSyncIrq_Reset(void)
{
    memset(&m_VSync, 0, sizeof(m_VSync));
}

void VSyncIrq_Edge(void)
{
    m_VSync.count1 = M_Increment(m_VSync.count1);
    m_VSync.count2 = M_Increment(m_VSync.count2);
    m_VSync.edge++;
}

void VSyncIrq_PollEdge(void)
{
    m_VSync.edge++;
}

void VSyncIrq_SwitchProgram(void)
{
    m_VSync.count1 = 1;
    m_VSync.count2 = 1;
}

void VSyncIrq_BeginWait(VSYNC_WAIT *const wait)
{
    wait->edge = m_VSync.edge;
}

bool VSyncIrq_WaitReady(const VSYNC_WAIT *const wait)
{
    return wait->edge != m_VSync.edge;
}

bool VSyncIrq_DelayReady(const uint16_t frames)
{
    if ((uint16_t)m_VSync.count1 < frames) {
        return false;
    }
    m_VSync.count1 = 0;
    return true;
}

void VSyncIrq_BeginDelay(void)
{
    m_VSync.count1 = 0;
}
