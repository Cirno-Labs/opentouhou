#include "tests/test.h"

#include "core/base/utils.h"
#include "pc98/pmd/lfo.h"

TEST(random_scaling_includes_limit_and_zero_limit_still_advances)
{
    uint16_t seed = 0;
    CHECK_EQ_INT(PmdRandom_Next(&seed, 65535), 6);
    CHECK_EQ_INT(seed, 3);
    CHECK_EQ_INT(PmdRandom_Next(&seed, 0), 0);
    CHECK_EQ_INT(seed, 780);
    seed = 14676;
    CHECK_EQ_INT(PmdRandom_Next(&seed, 65535), 65535);
    CHECK_EQ_INT(seed, 32767);
    seed = 65535;
    CHECK_EQ_INT(PmdRandom_Next(&seed, 32767), 32512);
}

TEST(triangle_primes_speed_and_doubles_period_at_reversal)
{
    PMD_LFO lfo;
    PmdLfo_Init(&lfo);
    const uint8_t parameters[] = { 0, 1, 1, 2 };
    PmdLfo_Configure(&lfo, parameters);
    uint16_t seed = 123;
    const uint16_t expected[] = { 0, 1, 2, 1, 0, 0xffff, 0xfffe };
    for (size_t i = 0; i < ARRAY_SIZE(expected); i++) {
        CHECK(PmdLfo_Step(&lfo, false, 0, &seed) == (i != 0));
        CHECK_EQ_INT(lfo.value, expected[i]);
    }
    CHECK_EQ_INT(lfo.period, 4);
    CHECK_EQ_INT(lfo.delta, 1);
    CHECK_EQ_INT(seed, 123);
}

TEST(reset_keeps_depth_rate_and_preserves_wrapping_speed_freeze)
{
    PMD_LFO lfo;
    PmdLfo_Init(&lfo);
    lfo.wave = 7;
    lfo.depth_counter = 42;
    lfo.initial_depth_count = 3;
    const uint8_t parameters[] = { 0, 255, 1, 4 };
    PmdLfo_Configure(&lfo, parameters);
    CHECK_EQ_INT(lfo.speed, 0);
    CHECK_EQ_INT(lfo.depth_counter, 42);
    CHECK_EQ_INT(lfo.depth_count, 3);
    uint16_t seed = 7;
    for (size_t i = 0; i < 100; i++) {
        CHECK(!PmdLfo_Step(&lfo, false, 0, &seed));
    }
    CHECK_EQ_INT(lfo.speed, 255);
    CHECK_EQ_INT(seed, 7);
    CHECK_EQ_INT(PmdLfo_Switch(&lfo, 0xe6, 255), 0xe1);
    CHECK_EQ_INT(PmdLfo_Switch(&lfo, 0xe6, 3), 0xe3);
    lfo.wave = 3;
    PmdLfo_Reset(&lfo);
    CHECK_EQ_INT(lfo.speed, 1);
}

TEST(timer_batch_reports_final_change_and_delay_decrements_once)
{
    PMD_LFO lfo;
    PmdLfo_Init(&lfo);
    lfo.wave = 2;
    const uint8_t parameters[] = { 1, 1, 1, 1 };
    PmdLfo_Configure(&lfo, parameters);
    uint16_t seed = 0;
    CHECK(!PmdLfo_Step(&lfo, true, 255, &seed));
    CHECK_EQ_INT(lfo.delay, 0);
    CHECK_EQ_INT(lfo.value, 0);
    CHECK(!PmdLfo_Step(&lfo, true, 0, &seed));
    CHECK(PmdLfo_Step(&lfo, true, 2, &seed));
    CHECK_EQ_INT(lfo.value, 0xffff);
    CHECK(!PmdLfo_Step(&lfo, true, 2, &seed));
    CHECK_EQ_INT(lfo.value, 0xffff);
}

TEST(wave_signed_byte_products_and_one_shot_stop)
{
    PMD_LFO lfo;
    PmdLfo_Init(&lfo);
    uint16_t seed = 0;
    lfo.wave = 2;
    const uint8_t square[] = { 0, 1, 128, 255 };
    PmdLfo_Configure(&lfo, square);
    CHECK(PmdLfo_Step(&lfo, false, 0, &seed));
    CHECK_EQ_INT(lfo.value, 128);
    lfo.wave = 5;
    PmdLfo_Reset(&lfo);
    CHECK(!PmdLfo_Step(&lfo, false, 0, &seed));
    CHECK(PmdLfo_Step(&lfo, false, 0, &seed));
    CHECK_EQ_INT(lfo.value, 16384);
    lfo.wave = 6;
    const uint8_t one_shot[] = { 0, 1, 2, 2 };
    PmdLfo_Configure(&lfo, one_shot);
    CHECK(!PmdLfo_Step(&lfo, false, 0, &seed));
    CHECK(PmdLfo_Step(&lfo, false, 0, &seed));
    CHECK(PmdLfo_Step(&lfo, false, 0, &seed));
    CHECK_EQ_INT(lfo.value, 4);
    CHECK(!PmdLfo_Step(&lfo, false, 0, &seed));
}

TEST(depth_adjusts_before_square_wave_reverses_delta)
{
    PMD_LFO lfo;
    PmdLfo_Init(&lfo);
    lfo.wave = 2;
    lfo.depth_counter = 1;
    lfo.depth_interval = 7;
    lfo.initial_depth_count = 1;
    const uint8_t parameters[] = { 0, 1, 128, 1 };
    PmdLfo_Configure(&lfo, parameters);
    uint16_t seed = 0;
    CHECK(PmdLfo_Step(&lfo, false, 0, &seed));
    CHECK_EQ_INT(lfo.value, 0xff80);
    CHECK_EQ_INT(lfo.delta, 127);
    CHECK_EQ_INT(lfo.depth_counter, 7);
    CHECK_EQ_INT(lfo.depth_count, 0);
}
