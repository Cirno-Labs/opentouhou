#include "th01/ending/ending.h"

#include "core/base/assert.h"

#include <string.h>

void Ending_Snapshot(END_RESULT *const result)
{
    ASSERT(g_Session.ending != SESSION_END_NONE);
    *result =
        (END_RESULT) { .score = g_Session.score,
                       .highest_score = g_Session.highest_score,
                       .random = g_Session.exit_rand,
                       .extra_lives = (int8_t)g_Session.settings.lives_extra,
                       .rank = (uint8_t)g_Session.settings.difficulty,
                       .route = g_Session.ending };
    for (int32_t i = 0; i < 4; i++) {
        result->continues[i] = g_Session.continues_per_scene[i];
        result->total += result->continues[i];
    }
    Session_ClearHistory();
}

void Ending_Clear(void)
{
    Session_ClearHistory();
    Session_ClearProgress();
}

void Ending_CalculateTitle(END_RESULT *const r)
{
    int32_t score;
    int32_t highest;
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
