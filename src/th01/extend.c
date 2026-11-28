#include "th01/extend.h"

#include "th01/session.h"
#include "th01/sound.h"

#include "pc98/zun/math16.h"

// Holds the extend threshold index and the life count that survives a
// Continue or a program switch.
static struct {
    int16_t next;
    int8_t resident_lives;
} m_Extend;

void Extend_Reset(void)
{
    m_Extend.next = 1;
}

int16_t Extend_NextForScore(const uint32_t resident_score)
{
    const int64_t score = resident_score <= INT32_MAX
        ? resident_score
        : (int64_t)resident_score - INT64_C(4294967296);
    return Math16_Wrap((int32_t)(score / 400000 + 1));
}

void Extend_ResetNext(const uint32_t resident_score)
{
    m_Extend.next = Extend_NextForScore(resident_score);
}

void Extend_SyncResidentLives(const int32_t lives)
{
    m_Extend.resident_lives = (int8_t)lives;
}

int32_t Extend_ResidentLives(void)
{
    return m_Extend.resident_lives;
}

bool Extend_Step(int16_t *const next, const uint32_t score)
{
    const uint32_t threshold = (uint32_t)(int32_t)*next * UINT32_C(400000);
    if (score < threshold) {
        return false;
    }
    *next = Math16_Wrap(*next + 1);
    return true;
}

void Extend_UpdateAndRender(void)
{
    if (!Extend_Step(&m_Extend.next, g_Stage->score)) {
        return;
    }
    int16_t *const lives = &g_Session.lives;
    if (*lives < 6) {
        (*lives)++;
        m_Extend.resident_lives = (int8_t)*lives;
        Hud_LivesPut((int16_t)(*lives - 1));
        Sound_Effect(SOUND_EFFECT_ITEM);
        Pellets_SpeedRaise(1);
    }
}
