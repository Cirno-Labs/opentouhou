#include "tests/test.h"

#include "stg/timer.h"

#include <stdint.h>
#include <string.h>

// The float after 0.99f.
#define M_JUST_OVER_0_99 0x1.fae14ap-1f

static uint32_t M_Bits(const float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static void M_TickMany(TIMER *const timer, const float speed, const int count)
{
    for (int i = 0; i < count; i++) {
        Timer_TickAt(timer, speed);
    }
}

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

TEST(init_and_set_clear_the_fraction)
{
    TIMER timer;
    Timer_Init(&timer);
    CHECK_EQ_INT(M_Bits(timer.sub_frame), 0);
    Timer_TickAt(&timer, 0.5f);
    Timer_Set(&timer, 4);
    CHECK_EQ_INT(timer.current, 4);
    CHECK_EQ_INT(M_Bits(timer.sub_frame), 0);
    Timer_TickAt(&timer, 0.5f);
    Timer_Init(&timer);
    CHECK_EQ_INT(timer.current, 0);
    CHECK_EQ_INT(timer.previous, -1);
    CHECK_EQ_INT(M_Bits(timer.sub_frame), 0);
}

TEST(tick_at_full_speed_adds_a_frame)
{
    TIMER timer;
    Timer_Set(&timer, 5);
    Timer_TickAt(&timer, 1.0f);
    CHECK_EQ_INT(timer.previous, 5);
    CHECK_EQ_INT(timer.current, 6);
    CHECK_EQ_INT(M_Bits(timer.sub_frame), 0);
    CHECK(Timer_HasTicked(&timer));
}

TEST(tick_at_half_speed_adds_a_frame_every_second_tick)
{
    TIMER timer;
    Timer_Set(&timer, 5);
    Timer_TickAt(&timer, 0.5f);
    CHECK_EQ_INT(timer.previous, 5);
    CHECK_EQ_INT(timer.current, 5);
    CHECK_EQ_INT(M_Bits(timer.sub_frame), 0x3f000000);
    CHECK(!Timer_HasTicked(&timer));
    Timer_TickAt(&timer, 0.5f);
    CHECK_EQ_INT(timer.previous, 5);
    CHECK_EQ_INT(timer.current, 6);
    CHECK_EQ_INT(M_Bits(timer.sub_frame), 0);
    CHECK(Timer_HasTicked(&timer));
    M_TickMany(&timer, 0.5f, 9998);
    CHECK_EQ_INT(timer.current, 5005);
    CHECK_EQ_INT(M_Bits(timer.sub_frame), 0);
}

TEST(tick_at_a_third_and_a_quarter_stay_exact)
{
    TIMER timer;
    Timer_Init(&timer);
    M_TickMany(&timer, 1.0f / 3.0f, 2);
    CHECK_EQ_INT(timer.current, 0);
    CHECK_EQ_INT(M_Bits(timer.sub_frame), 0x3f2aaaab);
    Timer_TickAt(&timer, 1.0f / 3.0f);
    CHECK_EQ_INT(timer.current, 1);
    CHECK_EQ_INT(M_Bits(timer.sub_frame), 0);
    M_TickMany(&timer, 1.0f / 3.0f, 9997);
    CHECK_EQ_INT(timer.current, 3333);
    CHECK_EQ_INT(M_Bits(timer.sub_frame), 0x3eaaaaab);

    Timer_Init(&timer);
    M_TickMany(&timer, 0.25f, 3);
    CHECK_EQ_INT(timer.current, 0);
    CHECK_EQ_INT(M_Bits(timer.sub_frame), 0x3f400000);
    M_TickMany(&timer, 0.25f, 9997);
    CHECK_EQ_INT(timer.current, 2500);
    CHECK_EQ_INT(M_Bits(timer.sub_frame), 0);
}

TEST(tick_at_rounds_in_single_precision)
{
    TIMER timer;
    Timer_Init(&timer);
    M_TickMany(&timer, 0.1f, 10);
    CHECK_EQ_INT(timer.current, 1);
    CHECK_EQ_INT(M_Bits(timer.sub_frame), 0x34000000);
    M_TickMany(&timer, 0.1f, 9990);
    CHECK_EQ_INT(timer.current, 1000);
    CHECK_EQ_INT(M_Bits(timer.sub_frame), 0x38fa0000);

    Timer_Init(&timer);
    M_TickMany(&timer, 0.7f, 10);
    CHECK_EQ_INT(timer.current, 6);
    CHECK_EQ_INT(M_Bits(timer.sub_frame), 0x3f7ffffb);
    M_TickMany(&timer, 0.7f, 9990);
    CHECK_EQ_INT(timer.current, 6999);
    CHECK_EQ_INT(M_Bits(timer.sub_frame), 0x3f7ff05f);
}

TEST(tick_at_counts_0_99_as_slow)
{
    TIMER timer;
    Timer_Init(&timer);
    Timer_TickAt(&timer, 0.99f);
    CHECK_EQ_INT(timer.current, 0);
    CHECK_EQ_INT(M_Bits(timer.sub_frame), M_Bits(0.99f));
    Timer_TickAt(&timer, M_JUST_OVER_0_99);
    CHECK_EQ_INT(timer.current, 1);
    CHECK_EQ_INT(M_Bits(timer.sub_frame), M_Bits(0.99f));
}

TEST(tick_at_carries_at_most_one_frame)
{
    TIMER timer;
    Timer_Init(&timer);
    timer.sub_frame = 1.5f;
    Timer_TickAt(&timer, 0.75f);
    CHECK_EQ_INT(timer.current, 1);
    CHECK_EQ_INT(M_Bits(timer.sub_frame), M_Bits(1.25f));
}

TEST(add_at_full_speed_changes_only_current)
{
    TIMER timer;
    Timer_Set(&timer, 10);
    Timer_TickAt(&timer, 0.5f);
    Timer_AddAt(&timer, 3, M_JUST_OVER_0_99);
    CHECK_EQ_INT(timer.previous, 10);
    CHECK_EQ_INT(timer.current, 13);
    CHECK_EQ_INT(M_Bits(timer.sub_frame), 0x3f000000);
    Timer_AddAt(&timer, -5, 1.0f);
    CHECK_EQ_INT(timer.previous, 10);
    CHECK_EQ_INT(timer.current, 8);
    CHECK_EQ_INT(M_Bits(timer.sub_frame), 0x3f000000);
}

TEST(sub_at_full_speed_changes_only_current)
{
    TIMER timer;
    Timer_Set(&timer, 10);
    Timer_TickAt(&timer, 0.5f);
    Timer_SubAt(&timer, 3, 1.0f);
    CHECK_EQ_INT(timer.previous, 10);
    CHECK_EQ_INT(timer.current, 7);
    CHECK_EQ_INT(M_Bits(timer.sub_frame), 0x3f000000);
    Timer_SubAt(&timer, -5, 1.0f);
    CHECK_EQ_INT(timer.previous, 10);
    CHECK_EQ_INT(timer.current, 12);
}

TEST(add_at_slow_speed_carries_every_whole_frame)
{
    TIMER timer;
    Timer_Set(&timer, 10);
    Timer_AddAt(&timer, 5, 0.5f);
    CHECK_EQ_INT(timer.previous, 10);
    CHECK_EQ_INT(timer.current, 12);
    CHECK_EQ_INT(M_Bits(timer.sub_frame), 0x3f000000);
    Timer_AddAt(&timer, 1, 0.5f);
    CHECK_EQ_INT(timer.previous, 12);
    CHECK_EQ_INT(timer.current, 13);
    CHECK_EQ_INT(M_Bits(timer.sub_frame), 0);
    Timer_AddAt(&timer, 1, 0.99f);
    CHECK_EQ_INT(timer.previous, 13);
    CHECK_EQ_INT(timer.current, 13);
    CHECK_EQ_INT(M_Bits(timer.sub_frame), M_Bits(0.99f));
    CHECK(!Timer_HasTicked(&timer));
}

TEST(add_at_multiplies_before_adding)
{
    TIMER timer;
    Timer_Init(&timer);
    Timer_AddAt(&timer, 10, 0.1f);
    CHECK_EQ_INT(timer.current, 1);
    CHECK_EQ_INT(M_Bits(timer.sub_frame), 0);

    Timer_Init(&timer);
    Timer_TickAt(&timer, 0.3f);
    Timer_AddAt(&timer, 6, 0.3f);
    CHECK_EQ_INT(timer.current, 2);
    CHECK_EQ_INT(M_Bits(timer.sub_frame), 0x3dcccce0);
}

TEST(add_at_slow_speed_sends_negative_frames_to_sub)
{
    TIMER timer;
    Timer_Set(&timer, 10);
    Timer_AddAt(&timer, -3, 0.5f);
    CHECK_EQ_INT(timer.previous, 10);
    CHECK_EQ_INT(timer.current, 8);
    CHECK_EQ_INT(M_Bits(timer.sub_frame), 0x3f000000);
}

TEST(sub_at_slow_speed_borrows_every_whole_frame)
{
    TIMER timer;
    Timer_Set(&timer, 10);
    Timer_SubAt(&timer, 2, 0.5f);
    CHECK_EQ_INT(timer.previous, 10);
    CHECK_EQ_INT(timer.current, 9);
    CHECK_EQ_INT(M_Bits(timer.sub_frame), 0);
    Timer_SubAt(&timer, 5, 0.5f);
    CHECK_EQ_INT(timer.previous, 9);
    CHECK_EQ_INT(timer.current, 6);
    CHECK_EQ_INT(M_Bits(timer.sub_frame), 0x3f000000);
    Timer_SubAt(&timer, 1, 0.5f);
    CHECK_EQ_INT(timer.previous, 6);
    CHECK_EQ_INT(timer.current, 6);
    CHECK_EQ_INT(M_Bits(timer.sub_frame), 0);
    CHECK(!Timer_HasTicked(&timer));
}

TEST(sub_at_counts_0_99_as_slow)
{
    TIMER timer;
    Timer_Set(&timer, 10);
    Timer_SubAt(&timer, 1, 0.99f);
    CHECK_EQ_INT(timer.previous, 10);
    CHECK_EQ_INT(timer.current, 9);
    CHECK_EQ_INT(M_Bits(timer.sub_frame), 0x3c23d700);
}

TEST(sub_at_multiplies_before_subtracting)
{
    TIMER timer;
    Timer_Set(&timer, 5);
    Timer_SubAt(&timer, 10, 0.1f);
    CHECK_EQ_INT(timer.current, 4);
    CHECK_EQ_INT(M_Bits(timer.sub_frame), 0);
}

TEST(sub_at_slow_speed_sends_negative_frames_to_add)
{
    TIMER timer;
    Timer_Set(&timer, 10);
    Timer_SubAt(&timer, -3, 0.5f);
    CHECK_EQ_INT(timer.previous, 10);
    CHECK_EQ_INT(timer.current, 11);
    CHECK_EQ_INT(M_Bits(timer.sub_frame), 0x3f000000);
}

TEST(as_float_adds_the_fraction_to_the_count)
{
    TIMER timer;
    Timer_Set(&timer, 10);
    Timer_TickAt(&timer, 0.25f);
    CHECK_EQ_INT(M_Bits(Timer_AsFloat(&timer)), M_Bits(10.25f));
    Timer_Set(&timer, 16777217);
    Timer_TickAt(&timer, 0.5f);
    CHECK_EQ_INT(M_Bits(Timer_AsFloat(&timer)), 0x4b800000);
}
