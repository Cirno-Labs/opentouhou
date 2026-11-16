#include "th01/extend.h"

int16_t Extend_NextForScore(const uint32_t resident_score)
{
    const int64_t score = resident_score <= INT32_MAX
        ? resident_score
        : (int64_t)resident_score - INT64_C(4294967296);
    return Math_Wrap16((int32_t)(score / 400000 + 1));
}

void Extend_UpdateAndRender(const EXTEND_CONTEXT *const c)
{
    const uint32_t threshold = (uint32_t)(int32_t)*c->next * UINT32_C(400000);
    if (c->display->gameplay->score < threshold) {
        return;
    }
    int16_t *const lives = c->display->remaining_lives;
    if (*lives < 6) {
        (*lives)++;
        *c->resident_lives = (int8_t)*lives;
        Hud_LivesPut(c->hud, c->display, (int16_t)(*lives - 1));
        c->effect(c->user, 15);
        Pellets_SpeedRaise(c->shared, 1);
    }
    *c->next = Math_Wrap16(*c->next + 1);
}
