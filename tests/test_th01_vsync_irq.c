#include "th01/vsync_irq.h"

#include "test.h"

TEST(raw_wait_requires_next_edge_and_delay_reuses_it)
{
    VSyncIrq_Reset();
    g_VSync.count1 = 37;
    g_VSync.count2 = -20;
    VSYNC_WAIT wait;
    VSyncIrq_BeginWait(&wait);
    CHECK(!VSyncIrq_WaitReady(&wait));
    CHECK(!VSyncIrq_WaitReady(&wait));
    CHECK_EQ_INT(g_VSync.count1, 37);
    VSyncIrq_Edge();
    CHECK(VSyncIrq_WaitReady(&wait));
    CHECK_EQ_INT(g_VSync.count1, 38);
    CHECK_EQ_INT(g_VSync.count2, -19);
    CHECK(VSyncIrq_DelayReady(1));
    CHECK_EQ_INT(g_VSync.count1, 0);
    CHECK_EQ_INT(g_VSync.count2, -19);
    CHECK(!VSyncIrq_DelayReady(1));
    VSyncIrq_BeginWait(&wait);
    CHECK(!VSyncIrq_WaitReady(&wait));
    VSyncIrq_Edge();
    CHECK(VSyncIrq_WaitReady(&wait));
    CHECK(VSyncIrq_DelayReady(1));
}

TEST(delay_resets_on_exit_and_preserves_unsigned_comparison)
{
    VSyncIrq_Reset();
    g_VSync.count1 = 1;
    g_VSync.count2 = 32767;
    CHECK(!VSyncIrq_DelayReady(2));
    CHECK_EQ_INT(g_VSync.count1, 1);
    VSyncIrq_Edge();
    CHECK_EQ_INT(g_VSync.count2, -32768);
    CHECK(VSyncIrq_DelayReady(2));
    CHECK_EQ_INT(g_VSync.count1, 0);
    g_VSync.count1 = 32767;
    VSyncIrq_Edge();
    CHECK_EQ_INT(g_VSync.count1, -32768);
    CHECK(VSyncIrq_DelayReady(32768));
    g_VSync.count1 = -1;
    CHECK(VSyncIrq_DelayReady(65535));
    g_VSync.count1 = -1;
    VSyncIrq_Edge();
    CHECK_EQ_INT(g_VSync.count1, 0);
    CHECK(!VSyncIrq_DelayReady(65535));
    g_VSync.count1 = 19;
    CHECK(VSyncIrq_DelayReady(0));
    CHECK_EQ_INT(g_VSync.count1, 0);
}

TEST(program_switch_counts_the_latched_vsync)
{
    VSyncIrq_Reset();
    g_VSync.count1 = 37;
    g_VSync.count2 = -20;
    g_VSync.edge = 5;
    VSyncIrq_SwitchProgram();
    CHECK_EQ_INT(g_VSync.count1, 1);
    CHECK_EQ_INT(g_VSync.count2, 1);
    CHECK_EQ_INT((int32_t)g_VSync.edge, 5);
    CHECK(VSyncIrq_DelayReady(1));
}
