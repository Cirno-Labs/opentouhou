#include "tests/test.h"
#include "th02/wait.h"

TEST(application_start_restarts_the_clock_but_program_handoffs_do_not)
{
    VSYNC_CLOCK clock = { .phase = 12345 };
    memset(&g_VSync, 0, sizeof(g_VSync));
    g_VSync.count1 = 65535;
    g_VSync.count2 = 65535;
    g_VSync.edges = 999;
    for (size_t application = 0; application < 3; application++) {
        VSyncIrq_BeginApplication(&clock);
        CHECK_EQ_INT(g_VSync.edges, 0);
        CHECK_EQ_INT(g_VSync.count1, 0);
        CHECK(!VSync_Advance(&clock));
        CHECK(VSync_Advance(&clock));
        VSyncIrq_VSync();
        CHECK_EQ_INT(g_VSync.count1, 1);
        VSyncIrq_BeginProgram();
        CHECK_EQ_INT(g_VSync.count1, 0);
        CHECK_EQ_INT(g_VSync.count2, 0);
        CHECK_EQ_INT(g_VSync.edges, 1);
        CHECK(VSync_Advance(&clock));
        VSyncIrq_VSync();
        CHECK_EQ_INT(g_VSync.edges, 2);
        CHECK_EQ_INT(g_VSync.count1, 1);
    }
}

TEST(interrupt_counters_wrap_independently)
{
    memset(&g_VSync, 0, sizeof(g_VSync));
    g_VSync.count1 = 65535;
    g_VSync.count2 = 32767;
    VSyncIrq_VSync();
    CHECK_EQ_INT(g_VSync.count1, 0);
    CHECK_EQ_INT(g_VSync.count2, 32768);
    CHECK_EQ_INT(g_VSync.edges, 1);
}

TEST(delay_discards_old_count_on_entry_and_retains_count_on_completion)
{
    memset(&g_VSync, 0, sizeof(g_VSync));
    g_VSync.count1 = 27;
    g_VSync.count2 = 51;
    WAIT delay;
    Wait_Delay(&delay, 2);
    CHECK_EQ_INT(g_VSync.count1, 0);
    CHECK_EQ_INT(g_VSync.count2, 51);
    CHECK(!Wait_Ready(&delay, nullptr, 0));
    VSyncIrq_VSync();
    CHECK(!Wait_Ready(&delay, nullptr, 0));
    VSyncIrq_VSync();
    CHECK(Wait_Ready(&delay, nullptr, 0));
    CHECK_EQ_INT(g_VSync.count1, 2);
    CHECK_EQ_INT(g_VSync.count2, 53);
    CHECK(Wait_Ready(&delay, nullptr, 0));
    Wait_Delay(&delay, 0);
    CHECK_EQ_INT(g_VSync.count1, 0);
    CHECK(Wait_Ready(&delay, nullptr, 0));
}

TEST(delay_comparison_keeps_all_unsigned_thresholds)
{
    memset(&g_VSync, 0, sizeof(g_VSync));
    WAIT delay;
    for (uint32_t frames = 0; frames <= UINT16_MAX; frames++) {
        Wait_Delay(&delay, (uint16_t)frames);
        g_VSync.count1 = (uint16_t)frames;
        CHECK(Wait_Ready(&delay, nullptr, 0));
        CHECK_EQ_INT(g_VSync.count1, frames);
        if (frames != 0) {
            g_VSync.count1--;
            CHECK(!Wait_Ready(&delay, nullptr, 0));
        }
    }
}

TEST(raw_wait_completes_on_a_counter_change_and_keeps_delay_counts)
{
    memset(&g_VSync, 0, sizeof(g_VSync));
    g_VSync.count1 = 65535;
    g_VSync.count2 = 31;
    WAIT wait;
    Wait_Edge(&wait);
    CHECK(!Wait_Ready(&wait, nullptr, 0));
    VSyncIrq_VSync();
    CHECK(Wait_Ready(&wait, nullptr, 0));
    CHECK_EQ_INT(g_VSync.count1, 0);
    CHECK_EQ_INT(g_VSync.count2, 32);
    g_VSync.count1 = wait.count;
    CHECK(!Wait_Ready(&wait, nullptr, 0));
    WAIT delay;
    Wait_Delay(&delay, 1);
    CHECK(!Wait_Ready(&delay, nullptr, 0));
}
