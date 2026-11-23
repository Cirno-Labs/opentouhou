#include "th01/frame.h"

#include "test.h"

TEST(raw_wait_requires_next_edge_and_delay_reuses_it)
{
    Frame_Reset();
    g_VSync.count1 = 37;
    g_VSync.count2 = -20;
    FRAME_WAIT wait;
    Frame_BeginWait(&wait);
    CHECK(!Frame_WaitReady(&wait));
    CHECK(!Frame_WaitReady(&wait));
    CHECK_EQ_INT(g_VSync.count1, 37);
    Frame_VSync();
    CHECK(Frame_WaitReady(&wait));
    CHECK_EQ_INT(g_VSync.count1, 38);
    CHECK_EQ_INT(g_VSync.count2, -19);
    CHECK(Frame_DelayReady(1));
    CHECK_EQ_INT(g_VSync.count1, 0);
    CHECK_EQ_INT(g_VSync.count2, -19);
    CHECK(!Frame_DelayReady(1));
    Frame_BeginWait(&wait);
    CHECK(!Frame_WaitReady(&wait));
    Frame_VSync();
    CHECK(Frame_WaitReady(&wait));
    CHECK(Frame_DelayReady(1));
}

TEST(delay_resets_on_exit_and_preserves_unsigned_comparison)
{
    Frame_Reset();
    g_VSync.count1 = 1;
    g_VSync.count2 = 32767;
    CHECK(!Frame_DelayReady(2));
    CHECK_EQ_INT(g_VSync.count1, 1);
    Frame_VSync();
    CHECK_EQ_INT(g_VSync.count2, -32768);
    CHECK(Frame_DelayReady(2));
    CHECK_EQ_INT(g_VSync.count1, 0);
    g_VSync.count1 = 32767;
    Frame_VSync();
    CHECK_EQ_INT(g_VSync.count1, -32768);
    CHECK(Frame_DelayReady(32768));
    g_VSync.count1 = -1;
    CHECK(Frame_DelayReady(65535));
    g_VSync.count1 = -1;
    Frame_VSync();
    CHECK_EQ_INT(g_VSync.count1, 0);
    CHECK(!Frame_DelayReady(65535));
    g_VSync.count1 = 19;
    CHECK(Frame_DelayReady(0));
    CHECK_EQ_INT(g_VSync.count1, 0);
}

TEST(program_switch_counts_the_latched_vsync)
{
    Frame_Reset();
    g_VSync.count1 = 37;
    g_VSync.count2 = -20;
    g_VSync.edge = 5;
    Frame_SwitchProgram();
    CHECK_EQ_INT(g_VSync.count1, 1);
    CHECK_EQ_INT(g_VSync.count2, 1);
    CHECK_EQ_INT((int32_t)g_VSync.edge, 5);
    CHECK(Frame_DelayReady(1));
}
