#include "th01/extend.h"
#include "th01/session.h"
#include "th01/sound.h"
#include "pc98/zun/math16.h"

EXTEND g_Extend;

int16_t Extend_NextForScore(const uint32_t resident_score)
{
    const int64_t score = resident_score <= INT32_MAX
        ? resident_score
        : (int64_t)resident_score - INT64_C(4294967296);
    return Math16_Wrap((int32_t)(score / 400000 + 1));
}

void Extend_UpdateAndRender(void)
{
    const uint32_t threshold =
        (uint32_t)(int32_t)g_Extend.next * UINT32_C(400000);
    if (g_Stage.score < threshold) {
        return;
    }
    int16_t *const lives = &g_Session.lives;
    if (*lives < 6) {
        (*lives)++;
        g_Extend.resident_lives = (int8_t)*lives;
        Hud_LivesPut((int16_t)(*lives - 1));
        Sound_Effect(15);
        Pellets_SpeedRaise(1);
    }
    g_Extend.next = Math16_Wrap(g_Extend.next + 1);
}
