#include "core/app/tick_rate.h"

#include "test.h"

#define NS_PER_MS 1000000ULL

TEST(first_call_starts_the_period)
{
    TICK_RATE meter = {};
    float rate = -1.0f;
    CHECK(!TickRate_Add(&meter, 0, 5000 * NS_PER_MS, &rate));
    CHECK(rate == -1.0f);
}

TEST(half_a_second_at_full_speed_measures_sixty)
{
    TICK_RATE meter = {};
    float rate = 0.0f;
    TickRate_Add(&meter, 0, 0, &rate);
    for (uint64_t ms = 1; ms < 500; ms++) {
        CHECK(
            !TickRate_Add(&meter, ms % 50 == 0 ? 3 : 0, ms * NS_PER_MS, &rate));
    }
    CHECK(TickRate_Add(&meter, 3, 500 * NS_PER_MS, &rate));
    CHECK(rate == 60.0f);
}

TEST(a_late_measurement_divides_by_the_whole_period)
{
    TICK_RATE meter = {};
    float rate = 0.0f;
    TickRate_Add(&meter, 0, 0, &rate);
    CHECK(TickRate_Add(&meter, 30, 1000 * NS_PER_MS, &rate));
    CHECK(rate == 30.0f);
}

TEST(each_measurement_starts_a_new_period)
{
    TICK_RATE meter = {};
    float rate = 0.0f;
    TickRate_Add(&meter, 0, 0, &rate);
    CHECK(TickRate_Add(&meter, 30, 500 * NS_PER_MS, &rate));
    CHECK(!TickRate_Add(&meter, 4, 999 * NS_PER_MS, &rate));
    CHECK(TickRate_Add(&meter, 6, 1000 * NS_PER_MS, &rate));
    CHECK(rate == 20.0f);
}

TEST(the_ticks_of_the_first_call_count)
{
    TICK_RATE meter = {};
    float rate = 0.0f;
    TickRate_Add(&meter, 10, 0, &rate);
    CHECK(TickRate_Add(&meter, 20, 500 * NS_PER_MS, &rate));
    CHECK(rate == 60.0f);
}

TEST(time_going_backwards_starts_a_new_period)
{
    TICK_RATE meter = {};
    float rate = 0.0f;
    TickRate_Add(&meter, 0, 5000 * NS_PER_MS, &rate);
    TickRate_Add(&meter, 25, 5400 * NS_PER_MS, &rate);
    CHECK(!TickRate_Add(&meter, 0, 0, &rate));
    CHECK(TickRate_Add(&meter, 15, 500 * NS_PER_MS, &rate));
    CHECK(rate == 30.0f);
}
