#include "th01/session.h"

#include <string.h>

SESSION g_Session;

void Session_Init(void)
{
    memset(&g_Session, 0, sizeof(g_Session));
}

void Session_Begin(
    const SESSION_BEGIN begin, const TH01_SETTINGS *const settings,
    const uint32_t frame_rand)
{
    if (begin == SESSION_START) {
        memset(&g_Session, 0, sizeof(g_Session));
    }
    g_Session.settings = *settings;
    g_Session.frame_rand = frame_rand;
    g_Session.exit_rand = frame_rand;
    g_Session.score = 0;
    g_Session.lives = (int16_t)(settings->lives_extra + 2);
    g_Session.bombs = (int16_t)settings->bombs;
    g_Session.pellet_speed = -4;
    g_Session.point_value = 0;
}

uint8_t Session_Scene(void)
{
    return g_Session.stage_id < 5
        ? 0
        : (uint8_t)((g_Session.stage_id / 5) * 2 - 1 + g_Session.route);
}

void Session_Store(
    const uint32_t score, const int16_t bombs, const int16_t pellet_speed)
{
    g_Session.score = score;
    g_Session.bombs = bombs;
    g_Session.pellet_speed = pellet_speed;
}

void Session_CountContinue(void)
{
    g_Session.continues_total =
        (int32_t)((uint32_t)g_Session.continues_total + 1);
    const int16_t scene = g_Session.stage_id / 5;
    g_Session.continues_per_scene[scene]++;
}

void Session_Continue(const int16_t pellet_speed, const int16_t lives)
{
    g_Session.point_value = 0;
    g_Session.pellet_speed = pellet_speed;
    g_Session.lives = lives;
}

void Session_ClearHistory(void)
{
    g_Session.score = 0;
    g_Session.highest_score = 0;
    g_Session.continues_total = 0;
    memset(
        g_Session.continues_per_scene, 0,
        sizeof(g_Session.continues_per_scene));
}

void Session_ClearProgress(void)
{
    g_Session.ending = SESSION_END_NONE;
    g_Session.stage_id = 0;
}
