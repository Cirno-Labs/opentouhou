#include "th01/frame.h"

#include "test.h"

TEST(raw_wait_requires_next_edge_and_delay_reuses_it)
{
    FRAME_VSYNC vsync = { .count1 = 37, .count2 = -20 };
    FRAME_WAIT wait;
    Frame_BeginWait(&wait, &vsync);
    CHECK(!Frame_WaitReady(&wait, &vsync));
    CHECK(!Frame_WaitReady(&wait, &vsync));
    CHECK_EQ_INT(vsync.count1, 37);
    Frame_VSync(&vsync);
    CHECK(Frame_WaitReady(&wait, &vsync));
    CHECK_EQ_INT(vsync.count1, 38);
    CHECK_EQ_INT(vsync.count2, -19);
    CHECK(Frame_DelayReady(&vsync, 1));
    CHECK_EQ_INT(vsync.count1, 0);
    CHECK_EQ_INT(vsync.count2, -19);
    CHECK(!Frame_DelayReady(&vsync, 1));
    Frame_BeginWait(&wait, &vsync);
    CHECK(!Frame_WaitReady(&wait, &vsync));
    Frame_VSync(&vsync);
    CHECK(Frame_WaitReady(&wait, &vsync));
    CHECK(Frame_DelayReady(&vsync, 1));
}

TEST(delay_resets_on_exit_and_preserves_unsigned_comparison)
{
    FRAME_VSYNC vsync = { .count1 = 1, .count2 = 32767 };
    CHECK(!Frame_DelayReady(&vsync, 2));
    CHECK_EQ_INT(vsync.count1, 1);
    Frame_VSync(&vsync);
    CHECK_EQ_INT(vsync.count2, -32768);
    CHECK(Frame_DelayReady(&vsync, 2));
    CHECK_EQ_INT(vsync.count1, 0);
    vsync.count1 = 32767;
    Frame_VSync(&vsync);
    CHECK_EQ_INT(vsync.count1, -32768);
    CHECK(Frame_DelayReady(&vsync, 32768));
    vsync.count1 = -1;
    CHECK(Frame_DelayReady(&vsync, 65535));
    vsync.count1 = -1;
    Frame_VSync(&vsync);
    CHECK_EQ_INT(vsync.count1, 0);
    CHECK(!Frame_DelayReady(&vsync, 65535));
    vsync.count1 = 19;
    CHECK(Frame_DelayReady(&vsync, 0));
    CHECK_EQ_INT(vsync.count1, 0);
}

TEST(program_switch_counts_the_latched_vsync)
{
    FRAME_VSYNC vsync = { .count1 = 37, .count2 = -20, .edge = 5 };
    Frame_SwitchProgram(&vsync);
    CHECK_EQ_INT(vsync.count1, 1);
    CHECK_EQ_INT(vsync.count2, 1);
    CHECK_EQ_INT((int32_t)vsync.edge, 5);
    CHECK(Frame_DelayReady(&vsync, 1));
}
