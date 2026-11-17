#include "th01/session.h"

#include "test.h"

TEST(start_clears_all_run_history_and_restores_credit)
{
    SESSION session;
    memset(&session, 0x55, sizeof(session));
    const TH01_SETTINGS settings = {
        .difficulty = SETTINGS_DIFFICULTY_HARD,
        .music = SETTINGS_MUSIC_FM,
        .bombs = 3,
        .lives_extra = 2,
    };
    Session_Begin(&session, SESSION_START, &settings, 0xfffffff0);
    CHECK_EQ_INT(session.score, 0);
    CHECK_EQ_INT(session.highest_score, 0);
    CHECK_EQ_INT(session.frame_rand, 0xfffffff0);
    CHECK_EQ_INT(session.route, SESSION_ROUTE_MAKAI);
    CHECK_EQ_INT(session.stage_id, 0);
    CHECK_EQ_INT(session.lives, 4);
    CHECK_EQ_INT(session.bombs, 3);
    CHECK_EQ_INT(session.pellet_speed, -4);
    CHECK_EQ_INT(session.point_value, 0);
    CHECK_EQ_INT(session.ending, SESSION_END_NONE);
    CHECK_EQ_INT(session.continues_total, 0);
    for (int32_t i = 0; i < SESSION_HISTORY_COUNT; i++) {
        CHECK_EQ_INT(session.continues_per_scene[i], 0);
        CHECK_EQ_INT(session.bonus_per_stage[i], 0);
    }
    CHECK_EQ_INT(session.settings.difficulty, SETTINGS_DIFFICULTY_HARD);
}

TEST(continue_preserves_run_history_with_fresh_credit)
{
    SESSION session = {
        .score = 54321,
        .highest_score = 654321,
        .stage_id = 14,
        .route = SESSION_ROUTE_JIGOKU,
        .ending = SESSION_END_JIGOKU,
        .continues_total = 17,
        .point_value = 6000,
        .pellet_speed = 19,
        .continues_per_scene = { 1, 2, 3, 4 },
        .bonus_per_stage = { -1, 1000, 30000, 500000 },
    };
    const TH01_SETTINGS settings = {
        .difficulty = SETTINGS_DIFFICULTY_EASY,
        .music = SETTINGS_MUSIC_OFF,
        .bombs = 1,
        .lives_extra = 4,
    };
    Session_Begin(&session, SESSION_CONTINUE, &settings, 42);
    CHECK_EQ_INT(session.score, 0);
    CHECK_EQ_INT(session.highest_score, 654321);
    CHECK_EQ_INT(session.frame_rand, 42);
    CHECK_EQ_INT(session.stage_id, 14);
    CHECK_EQ_INT(session.route, SESSION_ROUTE_JIGOKU);
    CHECK_EQ_INT(session.ending, SESSION_END_JIGOKU);
    CHECK_EQ_INT(session.continues_total, 17);
    CHECK_EQ_INT(session.point_value, 0);
    CHECK_EQ_INT(session.pellet_speed, -4);
    CHECK_EQ_INT(session.lives, 6);
    CHECK_EQ_INT(session.bombs, 1);
    const int32_t bonus[] = { -1, 1000, 30000, 500000 };
    for (int32_t i = 0; i < SESSION_HISTORY_COUNT; i++) {
        CHECK_EQ_INT(session.continues_per_scene[i], i + 1);
        CHECK_EQ_INT(session.bonus_per_stage[i], bonus[i]);
    }
}

TEST(fresh_continue_and_all_route_scene_selections)
{
    SESSION session = {};
    const TH01_SETTINGS settings = { .bombs = 2, .lives_extra = 2 };
    Session_Begin(&session, SESSION_CONTINUE, &settings, 0);
    CHECK_EQ_INT(session.route, SESSION_ROUTE_MAKAI);
    CHECK_EQ_INT(session.stage_id, 0);
    CHECK_EQ_INT(session.continues_total, 0);
    CHECK_EQ_INT(session.ending, SESSION_END_NONE);
    for (uint8_t route = 0; route < 2; route++) {
        session.route = route;
        for (int16_t stage = 0; stage < 20; stage++) {
            session.stage_id = stage;
            const int32_t expected = stage < 5 ? 0 : stage / 5 * 2 - 1 + route;
            CHECK_EQ_INT(Session_Scene(&session), expected);
        }
    }
}
