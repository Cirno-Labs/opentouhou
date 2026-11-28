#include "th01/session_private.h"

#include <string.h>

static SESSION m_Session;
const SESSION *const g_Session = &m_Session;
SESSION *const p_Session = &m_Session;

void Session_Init(void)
{
    memset(&m_Session, 0, sizeof(m_Session));
}

void Session_Begin(
    const SESSION_BEGIN begin, const TH01_SETTINGS *const settings,
    const uint32_t frame_rand)
{
    if (begin == SESSION_START) {
        memset(&m_Session, 0, sizeof(m_Session));
    }
    m_Session.settings = *settings;
    m_Session.frame_rand = frame_rand;
    m_Session.exit_rand = frame_rand;
    m_Session.score = 0;
    m_Session.lives = (int16_t)(settings->lives_extra + 2);
    m_Session.bombs = (int16_t)settings->bombs;
    m_Session.pellet_speed = -4;
    m_Session.point_value = 0;
}

uint8_t Session_Scene(void)
{
    return m_Session.stage_id < 5
        ? 0
        : (uint8_t)((m_Session.stage_id / 5) * 2 - 1 + m_Session.route);
}

void Session_Store(
    const uint32_t score, const int16_t bombs, const int16_t pellet_speed)
{
    m_Session.score = score;
    m_Session.bombs = bombs;
    m_Session.pellet_speed = pellet_speed;
}

void Session_CountContinue(void)
{
    m_Session.continues_total =
        (int32_t)((uint32_t)m_Session.continues_total + 1);
    const int16_t scene = m_Session.stage_id / 5;
    m_Session.continues_per_scene[scene]++;
}

void Session_Continue(const int16_t pellet_speed, const int16_t lives)
{
    m_Session.point_value = 0;
    m_Session.pellet_speed = pellet_speed;
    m_Session.lives = lives;
}

void Session_ClearHistory(void)
{
    m_Session.score = 0;
    m_Session.highest_score = 0;
    m_Session.continues_total = 0;
    memset(
        m_Session.continues_per_scene, 0,
        sizeof(m_Session.continues_per_scene));
}

void Session_ClearProgress(void)
{
    m_Session.ending = SESSION_END_NONE;
    m_Session.stage_id = 0;
}

uint16_t Session_RaisePointValue(void)
{
    if (m_Session.point_value < 59999) {
        m_Session.point_value =
            (uint16_t)(m_Session.point_value
                       + (m_Session.point_value < 10000 ? 1000 : 10000));
    }
    if (m_Session.point_value >= 60000) {
        m_Session.point_value = 65530;
    }
    return m_Session.point_value;
}

void Session_ResetPointValue(void)
{
    m_Session.point_value = 0;
}

bool Session_GrantLife(void)
{
    if (m_Session.lives >= 6) {
        return false;
    }
    m_Session.lives++;
    return true;
}

bool Session_LoseLife(void)
{
    if (m_Session.lives <= 0) {
        return false;
    }
    m_Session.lives--;
    return true;
}

void Session_AdvanceStage(void)
{
    m_Session.stage_id++;
}

void Session_MarkExitCheckpoint(void)
{
    m_Session.exit_rand = m_Session.frame_rand;
}

void Session_AdvanceFrameRand(void)
{
    m_Session.frame_rand++;
}

void Session_SyncHighScore(void)
{
    m_Session.high_score = m_Session.table_high_score;
}

bool Session_RecordHighScore(const uint32_t score)
{
    if (m_Session.high_score >= score) {
        return false;
    }
    m_Session.high_score = score;
    return true;
}

void Session_RecordVictory(const uint32_t stage_score)
{
    m_Session.ending = m_Session.route == SESSION_ROUTE_MAKAI
        ? SESSION_END_MAKAI
        : SESSION_END_JIGOKU;
    if (stage_score > m_Session.highest_score) {
        m_Session.highest_score = stage_score;
    }
}

void Session_BeginContinueRun(const uint32_t stage_score)
{
    if (m_Session.highest_score < stage_score) {
        m_Session.highest_score = stage_score;
    }
    m_Session.score = 0;
}

void Session_ChooseRoute(const uint8_t route)
{
    m_Session.route = route;
}

void Session_SyncTableHighScore(const uint32_t table_high_score)
{
    m_Session.table_high_score = table_high_score;
}

void Session_RecordStageBonus(const int32_t stage_number, const uint32_t bonus)
{
    m_Session.bonus_per_stage[(stage_number - 1) % 5] = bonus <= INT32_MAX
        ? (int32_t)bonus
        : (int32_t)((int64_t)bonus - INT64_C(4294967296));
}

void Session_RecordContinuesForScene(const int32_t stage_number)
{
    m_Session.continues_per_scene[stage_number / 5 - 1] =
        (uint16_t)m_Session.continues_total;
}
