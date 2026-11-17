#include "th01/ending.h"

#include "core/base/assert.h"

#include <string.h>

static void M_ClearHistory(SESSION *const session)
{
    session->score = 0;
    session->highest_score = 0;
    session->continues_total = 0;
    memset(
        session->continues_per_scene, 0, sizeof(session->continues_per_scene));
}

void Ending_Snapshot(END_RESULT *const result, SESSION *const session)
{
    ASSERT(session->ending != SESSION_END_NONE);
    *result =
        (END_RESULT) { .score = session->score,
                       .highest_score = session->highest_score,
                       .random = session->exit_rand,
                       .extra_lives = (int8_t)session->settings.lives_extra,
                       .rank = (uint8_t)session->settings.difficulty,
                       .route = session->ending };
    for (int32_t i = 0; i < 4; i++) {
        result->continues[i] = session->continues_per_scene[i];
        result->total += result->continues[i];
    }
    M_ClearHistory(session);
}

void Ending_Clear(SESSION *const session)
{
    M_ClearHistory(session);
    session->ending = SESSION_END_NONE;
    session->stage_id = 0;
}

void Ending_CalculateTitle(END_RESULT *const r)
{
    int32_t score, highest;
    memcpy(&score, &r->score, sizeof(score));
    memcpy(&highest, &r->highest_score, sizeof(highest));
    int32_t skill = 0;
    for (int32_t threshold = 500000; threshold <= 2500000;
         threshold += 500000) {
        if (score >= threshold) {
            skill += 2;
        }
    }
    for (int32_t threshold = 1400000; threshold <= 3000000;
         threshold += 400000) {
        if (highest >= threshold) {
            skill += 2;
        }
    }
    if (r->total == 0) {
        skill += 30;
    } else if (r->total > 20) {
        skill -= 10;
    } else {
        skill += 12 - (int32_t)((r->total + 1) / 2) * 2;
    }
    if (r->rank < 4) {
        skill += (int32_t)r->rank * 20 - 10;
    }
    if (r->route == SESSION_END_JIGOKU) {
        skill += 5;
    }
    if (r->extra_lives == 4) {
        skill -= 10;
    } else if (r->extra_lives == 3) {
        skill -= 5;
    } else if (r->extra_lives == 1) {
        skill += 5;
    } else if (r->extra_lives == 0) {
        skill += 10;
    }
    r->skill = skill;
    r->level = skill < 0 ? 0 : skill >= 80 ? 5 : skill / 20 + 1;
    r->random = r->random * 0x015a4e35u + 1;
    r->group = (int32_t)(((r->random >> 16) & 0x7fff) % 3);
}
