#include "tests/test.h"
#include "th01/session.h"

TEST(start_clears_all_run_history_and_restores_credit)
{
    memset(&g_Session, 0x55, sizeof(g_Session));
    const TH01_SETTINGS settings = {
        .difficulty = SETTINGS_DIFFICULTY_HARD,
        .music = SETTINGS_MUSIC_FM,
        .bombs = 3,
        .lives_extra = 2,
    };
    Session_Begin(SESSION_START, &settings, 0xfffffff0);
    CHECK_EQ_INT(g_Session.score, 0);
    CHECK_EQ_INT(g_Session.highest_score, 0);
    CHECK_EQ_INT(g_Session.frame_rand, 0xfffffff0);
    CHECK_EQ_INT(g_Session.exit_rand, 0xfffffff0);
    CHECK_EQ_INT(g_Session.route, SESSION_ROUTE_MAKAI);
    CHECK_EQ_INT(g_Session.stage_id, 0);
    CHECK_EQ_INT(g_Session.lives, 4);
    CHECK_EQ_INT(g_Session.bombs, 3);
    CHECK_EQ_INT(g_Session.pellet_speed, -4);
    CHECK_EQ_INT(g_Session.point_value, 0);
    CHECK_EQ_INT(g_Session.ending, SESSION_END_NONE);
    CHECK_EQ_INT(g_Session.continues_total, 0);
    for (int32_t i = 0; i < SESSION_HISTORY_COUNT; i++) {
        CHECK_EQ_INT(g_Session.continues_per_scene[i], 0);
        CHECK_EQ_INT(g_Session.bonus_per_stage[i], 0);
    }
    CHECK_EQ_INT(g_Session.settings.difficulty, SETTINGS_DIFFICULTY_HARD);
}

TEST(continue_preserves_run_history_with_fresh_credit)
{
    Session_Init();
    g_Session.score = 54321;
    g_Session.highest_score = 654321;
    g_Session.stage_id = 14;
    g_Session.route = SESSION_ROUTE_JIGOKU;
    g_Session.ending = SESSION_END_JIGOKU;
    g_Session.continues_total = 17;
    g_Session.point_value = 6000;
    g_Session.pellet_speed = 19;
    memcpy(
        g_Session.continues_per_scene, (uint16_t[]) { 1, 2, 3, 4 },
        sizeof(g_Session.continues_per_scene));
    memcpy(
        g_Session.bonus_per_stage, (int32_t[]) { -1, 1000, 30000, 500000 },
        sizeof(g_Session.bonus_per_stage));
    const TH01_SETTINGS settings = {
        .difficulty = SETTINGS_DIFFICULTY_EASY,
        .music = SETTINGS_MUSIC_OFF,
        .bombs = 1,
        .lives_extra = 4,
    };
    Session_Begin(SESSION_CONTINUE, &settings, 42);
    CHECK_EQ_INT(g_Session.score, 0);
    CHECK_EQ_INT(g_Session.highest_score, 654321);
    CHECK_EQ_INT(g_Session.frame_rand, 42);
    CHECK_EQ_INT(g_Session.exit_rand, 42);
    CHECK_EQ_INT(g_Session.stage_id, 14);
    CHECK_EQ_INT(g_Session.route, SESSION_ROUTE_JIGOKU);
    CHECK_EQ_INT(g_Session.ending, SESSION_END_JIGOKU);
    CHECK_EQ_INT(g_Session.continues_total, 17);
    CHECK_EQ_INT(g_Session.point_value, 0);
    CHECK_EQ_INT(g_Session.pellet_speed, -4);
    CHECK_EQ_INT(g_Session.lives, 6);
    CHECK_EQ_INT(g_Session.bombs, 1);
    const int32_t bonus[] = { -1, 1000, 30000, 500000 };
    for (int32_t i = 0; i < SESSION_HISTORY_COUNT; i++) {
        CHECK_EQ_INT(g_Session.continues_per_scene[i], i + 1);
        CHECK_EQ_INT(g_Session.bonus_per_stage[i], bonus[i]);
    }
}

TEST(fresh_continue_and_all_route_scene_selections)
{
    Session_Init();
    const TH01_SETTINGS settings = { .bombs = 2, .lives_extra = 2 };
    Session_Begin(SESSION_CONTINUE, &settings, 0);
    CHECK_EQ_INT(g_Session.route, SESSION_ROUTE_MAKAI);
    CHECK_EQ_INT(g_Session.stage_id, 0);
    CHECK_EQ_INT(g_Session.continues_total, 0);
    CHECK_EQ_INT(g_Session.ending, SESSION_END_NONE);
    for (uint8_t route = 0; route < 2; route++) {
        g_Session.route = route;
        for (int16_t stage = 0; stage < 20; stage++) {
            g_Session.stage_id = stage;
            const int32_t expected = stage < 5 ? 0 : stage / 5 * 2 - 1 + route;
            CHECK_EQ_INT(Session_Scene(), expected);
        }
    }
}
