#include "tests/test.h"
#include "th02/session.h"

#include <string.h>

static TH02_SESSION M_Seed(void)
{
    return (TH02_SESSION) {
        .stage = 3,
        .debug = 1,
        .score = 1234567,
        .continues_used = 7,
        .bombs = 4,
        .lives = 5,
        .rank = 3,
        .start_power = 64,
        .music = 2,
        .start_bombs = 1,
        .start_lives = 2,
        .frame = -12345678,
        .unused_1 = 1234,
        .unused_2 = 17,
        .opening_return = 4,
        .reduce_effects = true,
        .unused_3 = 18,
        .shot_type = 2,
        .demo_num = 3,
        .skill = -300,
        .unused_4 = -123,
        .highest_score = 7654321,
    };
}

static void M_Check(
    const TH02_SESSION *const actual, const TH02_SESSION *const expected)
{
    CHECK_EQ_INT(actual->stage, expected->stage);
    CHECK_EQ_INT(actual->debug, expected->debug);
    CHECK_EQ_INT(actual->score, expected->score);
    CHECK_EQ_INT(actual->continues_used, expected->continues_used);
    CHECK_EQ_INT(actual->bombs, expected->bombs);
    CHECK_EQ_INT(actual->lives, expected->lives);
    CHECK_EQ_INT(actual->rank, expected->rank);
    CHECK_EQ_INT(actual->start_power, expected->start_power);
    CHECK_EQ_INT(actual->music, expected->music);
    CHECK_EQ_INT(actual->start_bombs, expected->start_bombs);
    CHECK_EQ_INT(actual->start_lives, expected->start_lives);
    CHECK_EQ_INT(actual->frame, expected->frame);
    CHECK_EQ_INT(actual->unused_1, expected->unused_1);
    CHECK_EQ_INT(actual->unused_2, expected->unused_2);
    CHECK_EQ_INT(actual->opening_return, expected->opening_return);
    CHECK_EQ_INT(actual->reduce_effects, expected->reduce_effects);
    CHECK_EQ_INT(actual->unused_3, expected->unused_3);
    CHECK_EQ_INT(actual->shot_type, expected->shot_type);
    CHECK_EQ_INT(actual->demo_num, expected->demo_num);
    CHECK_EQ_INT(actual->skill, expected->skill);
    CHECK_EQ_INT(actual->unused_4, expected->unused_4);
    CHECK_EQ_INT(actual->highest_score, expected->highest_score);
}

TEST(initialization_clears_the_native_session)
{
    TH02_SESSION session;
    memset(&session, 0xa5, sizeof(session));
    Session_Init(&session);
    const uint8_t *const bytes = (const uint8_t *)&session;
    for (size_t i = 0; i < sizeof(session); i++) {
        CHECK_EQ_INT(bytes[i], 0);
    }
}

TEST(credit_start_defers_partial_reset_and_keeps_persistent_options)
{
    for (int32_t extra = 0; extra < 2; extra++) {
        TH02_SESSION session = M_Seed();
        const TH02_SESSION before = session;
        TH02_SETTINGS settings = { 2, 0, 0, 4, false, true };
        SESSION_EVENTS events = Session_BeginStart(extra != 0);
        CHECK_EQ_INT(events.force_effect, SOUND_EFFECT_ITEM);
        CHECK_EQ_INT(events.wait_frames, 20);
        CHECK(
            !events.save_settings && !events.select_shot && !events.enter_main);
        M_Check(&session, &before);
        settings.music = 2;
        events = Session_FinishStart(&session, &settings);
        CHECK_EQ_INT(events.force_effect, SOUND_EFFECT_NONE);
        CHECK_EQ_INT(events.wait_frames, 0);
        CHECK(events.save_settings && events.select_shot && !events.enter_main);
        TH02_SESSION expected = before;
        expected.stage = extra != 0 ? 5 : 0;
        expected.rank = extra != 0 ? 4 : 2;
        expected.music = 2;
        expected.bombs = expected.start_bombs = extra != 0 ? 1 : 0;
        expected.lives = expected.start_lives = extra != 0 ? 2 : 4;
        expected.score = expected.highest_score = 0;
        expected.continues_used = 0;
        expected.unused_1 = expected.unused_3 = 0;
        expected.start_power = 0;
        expected.demo_num = 0;
        M_Check(&session, &expected);
        CHECK_EQ_INT(settings.difficulty, 2);
        CHECK_EQ_INT(settings.music, 2);
        CHECK_EQ_INT(settings.bombs, 0);
        CHECK_EQ_INT(settings.lives, 4);
        CHECK(!settings.reduce_effects);
        events = Session_FinishStart(&session, &settings);
        CHECK_EQ_INT(events.force_effect, SOUND_EFFECT_NONE);
        CHECK_EQ_INT(events.wait_frames, 0);
        CHECK(
            !events.save_settings && !events.select_shot && !events.enter_main);
        M_Check(&session, &expected);
    }
}

TEST(cancelled_start_cannot_apply_an_old_extra_credit_to_a_new_run)
{
    TH02_SESSION session = M_Seed();
    const TH02_SESSION before = session;
    const TH02_SETTINGS settings = { .difficulty = 2, .lives = 4, .bombs = 3 };
    Session_BeginStart(true);
    Session_CancelStart();
    const SESSION_EVENTS cancelled = Session_FinishStart(&session, &settings);
    CHECK(!cancelled.save_settings && !cancelled.select_shot);
    M_Check(&session, &before);
    Session_BeginStart(false);
    CHECK(Session_FinishStart(&session, &settings).select_shot);
    CHECK_EQ_INT(session.stage, 0);
    CHECK_EQ_INT(session.rank, 2);
    CHECK_EQ_INT(session.lives, 4);
}

TEST(demo_start_retains_prior_score_stage_power_and_highest_score)
{
    TH02_SESSION session = M_Seed();
    TH02_SESSION expected = session;
    const TH02_SETTINGS settings = { 3, 0, 0, 4, false, true };
    const SESSION_EVENTS events = Session_BeginDemo(&session, &settings, 255);
    CHECK_EQ_INT(events.force_effect, SOUND_EFFECT_NONE);
    CHECK_EQ_INT(events.wait_frames, 0);
    CHECK(events.save_settings && !events.select_shot && events.enter_main);
    expected.bombs = expected.start_bombs = 3;
    expected.lives = expected.start_lives = 2;
    expected.music = 0;
    expected.rank = 1;
    expected.continues_used = 0;
    expected.unused_3 = 0;
    expected.demo_num = -1;
    expected.shot_type = 0;
    M_Check(&session, &expected);
}

TEST(main_entry_uses_start_counters_and_preserves_nonzero_power_bits)
{
    TH02_SESSION session = M_Seed();
    session.stage = 0x80;
    session.start_lives = 0xff;
    session.start_bombs = 0x80;
    for (uint16_t power = 0; power < 256; power++) {
        session.start_power = (int8_t)power;
        SESSION_ENTRY entry;
        memset(&entry, 0xa5, sizeof(entry));
        Session_EnterMain(&session, &entry);
        CHECK_EQ_INT(entry.stage, -128);
        CHECK_EQ_INT(entry.lives, -1);
        CHECK_EQ_INT(entry.bombs, -128);
        CHECK_EQ_INT(entry.rank, 3);
        CHECK_EQ_INT(entry.power, power == 0 ? 1 : power);
        CHECK_EQ_INT(entry.performance, 0);
        CHECK_EQ_INT(entry.big_power_override, 0);
        CHECK_EQ_INT(session.start_power, (int8_t)power);
    }
}
