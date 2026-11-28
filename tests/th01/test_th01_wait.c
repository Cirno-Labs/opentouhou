#include "tests/test.h"
#include "th01/video.h"
#include "th01/video_private.h"
#include "th01/wait.h"

#include <string.h>

TEST(idle_wait_is_ready_at_once)
{
    WAIT wait;
    memset(&wait, 0, sizeof(wait));
    CHECK(Wait_Idle(&wait));
    CHECK(Wait_Ready(&wait, 0));
    CHECK(Wait_Idle(&wait));
}

TEST(delay_polls_once_and_arms_on_failure)
{
    VSyncIrq_Reset();
    WAIT wait;
    memset(&wait, 0, sizeof(wait));
    CHECK(!Wait_Delay(&wait, 2));
    CHECK(!Wait_Idle(&wait));
    CHECK(!Wait_Ready(&wait, 0));
    VSyncIrq_Edge();
    CHECK(!Wait_Ready(&wait, 0));
    VSyncIrq_Edge();
    CHECK(Wait_Ready(&wait, 0));
    CHECK(Wait_Idle(&wait));
}

TEST(delay_succeeds_at_once_without_arming)
{
    VSyncIrq_Reset();
    WAIT wait;
    memset(&wait, 0, sizeof(wait));
    CHECK(Wait_Delay(&wait, 0));
    CHECK(Wait_Idle(&wait));
}

TEST(scroll_always_arms_and_sets_scroll_on_the_next_edge)
{
    VSyncIrq_Reset();
    WAIT wait;
    memset(&wait, 0, sizeof(wait));
    CHECK(!Wait_Scroll(&wait, 37));
    CHECK(!Wait_Idle(&wait));
    CHECK(!Wait_Ready(&wait, 0));
    VSyncIrq_Edge();
    p_Video->scroll_top = 0;
    CHECK(Wait_Ready(&wait, 0));
    CHECK(Wait_Idle(&wait));
    CHECK_EQ_INT(g_Video->scroll_top, 37);
}

TEST(ticks_waits_the_given_host_ticks)
{
    WAIT wait;
    memset(&wait, 0, sizeof(wait));
    CHECK(!Wait_Ticks(&wait, 100, 6));
    CHECK(!Wait_Idle(&wait));
    CHECK(!Wait_Ready(&wait, 103));
    CHECK(!Wait_Ready(&wait, 105));
    CHECK(Wait_Ready(&wait, 106));
    CHECK(Wait_Idle(&wait));
}

TEST(ticks_of_zero_is_ready_at_once)
{
    WAIT wait;
    memset(&wait, 0, sizeof(wait));
    CHECK(Wait_Ticks(&wait, 100, 0));
    CHECK(Wait_Idle(&wait));
}
