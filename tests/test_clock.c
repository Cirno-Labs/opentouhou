#include "shell/game/clock.h"

#include "test.h"

#define NS_PER_SECOND 1000000000ULL
#define TICK_NS (NS_PER_SECOND / 60 + 1)

TEST(first_call_starts_the_clock)
{
    CLOCK clock;
    Clock_Init(&clock, 60, 4);
    CHECK_EQ_INT(Clock_Advance(&clock, 5 * NS_PER_SECOND), 0);
}

TEST(one_second_is_sixty_ticks)
{
    CLOCK clock;
    Clock_Init(&clock, 60, 1000);
    Clock_Advance(&clock, 0);
    int total = 0;
    for (uint64_t ms = 1; ms <= 1000; ms++) {
        total += Clock_Advance(&clock, ms * 1000000ULL);
    }
    CHECK_EQ_INT(total, 60);
}

TEST(catch_up_is_capped)
{
    CLOCK clock;
    Clock_Init(&clock, 60, 4);
    Clock_Advance(&clock, 0);
    CHECK_EQ_INT(Clock_Advance(&clock, NS_PER_SECOND), 4);
    CHECK_EQ_INT(Clock_Advance(&clock, NS_PER_SECOND + TICK_NS), 1);
}

TEST(time_going_backwards_restarts_the_clock)
{
    CLOCK clock;
    Clock_Init(&clock, 60, 4);
    Clock_Advance(&clock, NS_PER_SECOND);
    CHECK_EQ_INT(Clock_Advance(&clock, 0), 0);
    CHECK_EQ_INT(Clock_Advance(&clock, TICK_NS), 1);
}

TEST(time_until_next_tick)
{
    CLOCK clock;
    Clock_Init(&clock, 60, 4);
    Clock_Advance(&clock, 0);
    CHECK_EQ_INT(Clock_NsUntilNextTick(&clock), TICK_NS);
    Clock_Advance(&clock, TICK_NS - 1000);
    CHECK_EQ_INT(Clock_NsUntilNextTick(&clock), 1000);
}
