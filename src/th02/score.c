#include "th02/score.h"

#include "th02/hud.h"
#include "th02/sound_effects.h"

#include "core/base/assert.h"

#include <string.h>

static SCORE m_Score;
const SCORE *const g_Score = &m_Score;

static const int32_t m_Thresholds[] = { 100000, 200000, 300000,
                                        500000, 800000, 99999999 };

static int32_t M_Threshold(const uint16_t index)
{
    ASSERT(index < 8);
    if (index < ARRAY_SIZE(m_Thresholds)) {
        return m_Thresholds[index];
    }
    // The next two reads span the live extend index, reset value and the
    // first HUD divisor. Their word alignment differs from these C fields.
    const uint32_t reset = (uint32_t)m_Score.reset_unknown;
    return (int32_t)(index == 6 ? (reset << 16) | m_Score.extends
                                : (1000000u << 16) | (reset >> 16));
}

static void M_Render(const bool bombs)
{
    ASSERT(m_Score.extends < 8);
    Hud_Score(6, m_Score.value);
    if (m_Score.value > m_Score.highest) {
        m_Score.highest = m_Score.value;
        Hud_Score(4, m_Score.highest);
    }
    if (M_Threshold(m_Score.extends) <= m_Score.value) {
        if (m_Score.lives < 5) {
            m_Score.lives++;
            Hud_Lives(m_Score.lives);
            SoundEffects_Play(SOUND_EFFECT_EXTEND);
        } else if (bombs) {
            if (m_Score.bombs < 5) {
                m_Score.bombs++;
                Hud_Bombs(m_Score.bombs);
            }
            SoundEffects_Play(SOUND_EFFECT_EXTEND);
        }
        m_Score.extends++;
    }
}

void Score_Init(void)
{
    memset(&m_Score, 0, sizeof(m_Score));
}

void Score_InitHighest(const int32_t stored_score)
{
    const int32_t highest = stored_score / 10;
    m_Score.highest = highest >= m_Score.value ? highest : m_Score.value;
    m_Score.highest_continues = (uint8_t)(stored_score % 10);
}

void Score_InitExtends(void)
{
    ASSERT(m_Score.value < M_Threshold(7));
    m_Score.extends = 0;
    while (M_Threshold(m_Score.extends) <= m_Score.value) {
        m_Score.extends++;
    }
    m_Score.delta = 0;
}

void Score_Commit(void)
{
    m_Score.value =
        (int32_t)((uint32_t)m_Score.value + (uint32_t)m_Score.delta);
    m_Score.delta = 0;
    m_Score.transferred = 0;
}

void Score_Reset(void)
{
    m_Score.value = 0;
    m_Score.delta = 0;
    m_Score.transferred = 0;
    m_Score.reset_unknown = 40000;
}

void Score_Update(const bool reduce_effects)
{
    if (m_Score.delta == 0) {
        m_Score.transferred = 0;
        return;
    }
    if (reduce_effects) {
        m_Score.transferred = 3111;
    } else {
        int32_t transfer = m_Score.delta < 32 ? 32 : m_Score.delta;
        transfer /= 32;
        if (m_Score.transferred < transfer) {
            m_Score.transferred = transfer > 1111 ? 1111 : transfer;
        }
    }
    int32_t remaining =
        (int32_t)((uint32_t)m_Score.delta - m_Score.transferred);
    if (remaining < 0) {
        m_Score.transferred = (uint16_t)m_Score.delta;
        remaining = 0;
    }
    m_Score.delta = remaining;
    m_Score.value = (int32_t)((uint32_t)m_Score.value + m_Score.transferred);
    M_Render(true);
}

void Score_GrantBonus(void)
{
    m_Score.value =
        (int32_t)((uint32_t)m_Score.value + (uint32_t)m_Score.delta);
    M_Render(false);
}

void Score_BeginSession(
    const int32_t value, const int8_t lives, const int8_t bombs)
{
    m_Score.value = value;
    m_Score.delta = 0;
    m_Score.lives = lives;
    m_Score.bombs = bombs;
}

void Score_Continue(const int8_t lives, const int8_t bombs)
{
    m_Score.lives = lives;
    m_Score.bombs = bombs;
}

void Score_GrantBomb(void)
{
    m_Score.bombs++;
}

void Score_GrantLife(void)
{
    m_Score.lives++;
}

void Score_LoseLife(const int8_t start_bombs)
{
    m_Score.lives = (int8_t)(uint8_t)(m_Score.lives - 1);
    m_Score.bombs = start_bombs;
    if (m_Score.lives == 0) {
        m_Score.bombs = (int8_t)(uint8_t)(m_Score.bombs + 2);
    }
}

void Score_UseBomb(void)
{
    m_Score.bombs = (int8_t)(uint8_t)(m_Score.bombs - 1);
}

void Score_AddPoints(const uint32_t points)
{
    m_Score.delta = (int32_t)((uint32_t)m_Score.delta + points);
}

void Score_AddBonus(const uint32_t points)
{
    m_Score.value = (int32_t)((uint32_t)m_Score.value + points);
}

void Score_Set(const int32_t value)
{
    m_Score.value = value;
}

void Score_ClearDelta(void)
{
    m_Score.delta = 0;
}

void Score_AddDamage(const int16_t damage)
{
    const uint16_t low = (uint16_t)((uint16_t)m_Score.delta + damage);
    m_Score.delta = (int32_t)(((uint32_t)m_Score.delta & 0xffff0000u) | low);
}
