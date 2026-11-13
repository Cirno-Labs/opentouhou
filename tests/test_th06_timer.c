// SPDX-License-Identifier: GPL-3.0-only

#include "th06/timer.h"

#include "test.h"

TEST(init_has_ticked_from_no_count)
{
    TIMER timer;
    Timer_Init(&timer);
    CHECK_EQ_INT(timer.current, 0);
    CHECK_EQ_INT(timer.previous, -1);
    CHECK(Timer_HasTicked(&timer));
}

TEST(set_has_ticked_from_no_count)
{
    TIMER timer;
    Timer_Init(&timer);
    Timer_Set(&timer, 30);
    CHECK_EQ_INT(timer.current, 30);
    CHECK_EQ_INT(timer.previous, -999);
    CHECK(Timer_HasTicked(&timer));
}

TEST(tick_keeps_the_old_count)
{
    TIMER timer;
    Timer_Init(&timer);
    Timer_Tick(&timer);
    CHECK_EQ_INT(timer.current, 1);
    CHECK_EQ_INT(timer.previous, 0);
    CHECK(Timer_HasTicked(&timer));
}

TEST(adding_directly_keeps_previous)
{
    TIMER timer;
    Timer_Init(&timer);
    Timer_Tick(&timer);
    timer.current += 2;
    CHECK_EQ_INT(timer.previous, 0);
    timer.current -= 3;
    CHECK(!Timer_HasTicked(&timer));
}
