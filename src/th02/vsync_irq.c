#include "th02/vsync_irq.h"

#include <string.h>

VSYNC_COUNTERS g_VSync;

void VSyncIrq_BeginApplication(VSYNC_CLOCK *const clock)
{
    memset(clock, 0, sizeof(*clock));
    memset(&g_VSync, 0, sizeof(g_VSync));
}

void VSyncIrq_BeginProgram(void)
{
    g_VSync.count1 = g_VSync.count2 = 0;
}

void VSyncIrq_VSync(void)
{
    g_VSync.edges++;
    g_VSync.count1 = (uint16_t)(g_VSync.count1 + 1u);
    g_VSync.count2 = (uint16_t)(g_VSync.count2 + 1u);
}
