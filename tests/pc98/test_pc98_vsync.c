#include "pc98/vsync.h"

#include "test.h"

static uint64_t M_ReferenceCount(const uint64_t host_ticks)
{
    return host_ticks * 21052600 / (60 * 8 * 106 * 440);
}

TEST(first_vsync_waits_for_a_complete_reference_period)
{
    VSYNC_CLOCK clock = {};
    CHECK(!VSync_Advance(&clock));
    CHECK_EQ_INT(clock.phase, 105263);
    CHECK(VSync_Advance(&clock));
    CHECK_EQ_INT(clock.phase, 98590);
    for (int32_t tick = 3; tick <= 16; tick++) {
        CHECK(VSync_Advance(&clock));
    }
    CHECK(!VSync_Advance(&clock));
    CHECK(VSync_Advance(&clock));
}

TEST(each_prefix_matches_the_reference_dot_clock)
{
    VSYNC_CLOCK clock = {};
    uint64_t frames = 0;
    for (uint64_t tick = 1; tick <= 1000000; tick++) {
        frames += VSync_Advance(&clock);
        CHECK_EQ_INT(frames, M_ReferenceCount(tick));
        CHECK(clock.phase < 111936);
    }
}

TEST(full_periods_keep_the_exact_rate_without_drift)
{
    VSYNC_CLOCK clock = {};
    int32_t frames = 0;
    for (int32_t period = 1; period <= 3; period++) {
        for (int32_t tick = 0; tick < 111936; tick++) {
            frames += VSync_Advance(&clock);
        }
        CHECK_EQ_INT(frames, period * 105263);
        CHECK_EQ_INT(clock.phase, 0);
    }
}

TEST(restoring_phase_repeats_the_same_frame_boundaries)
{
    VSYNC_CLOCK first = {};
    for (int32_t tick = 0; tick < 137; tick++) {
        VSync_Advance(&first);
    }
    VSYNC_CLOCK restored = first;
    for (int32_t tick = 0; tick < 111936; tick++) {
        CHECK_EQ_INT(VSync_Advance(&first), VSync_Advance(&restored));
        CHECK_EQ_INT(first.phase, restored.phase);
    }
}
