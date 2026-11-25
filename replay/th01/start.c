#include "start.h"

#include "harness.h"

#include "th01/session.h"

#include "core/base/utils.h"

void Th01Start_Apply(const START_VALUE *const starts, const size_t count)
{
    bool scene_set = false;
    uint16_t scene_continues = 0;
    for (size_t i = 0; i < count; i++) {
        const uint32_t value = starts[i].value;
        switch (starts[i].field) {
        case TH01_START_stage:
            g_Session.stage_id = (int16_t)value;
            break;
        case TH01_START_route:
            g_Session.route = (uint8_t)value;
            break;
        case TH01_START_difficulty:
            g_Session.settings.difficulty = (int32_t)value;
            break;
        case TH01_START_lives:
            g_Session.lives = (int16_t)value;
            break;
        case TH01_START_bombs:
            g_Session.bombs = (int16_t)value;
            break;
        case TH01_START_score:
            g_Session.score = value;
            break;
        case TH01_START_continues:
            g_Session.continues_total = (int32_t)value;
            break;
        case TH01_START_pellet_speed:
            g_Session.pellet_speed = (int16_t)value;
            break;
        case TH01_START_boss_hp:
            break;
        case TH01_START_scene_continues:
            scene_set = true;
            scene_continues = (uint16_t)value;
            break;
        }
    }
    const size_t scene = (size_t)g_Session.stage_id / 5;
    if (scene_set && scene < ARRAY_SIZE(g_Session.continues_per_scene)) {
        g_Session.continues_per_scene[scene] = scene_continues;
    }
}

void Th01Start_ApplyBoss(
    const START_VALUE *const starts, const size_t count, int16_t *const hp)
{
    for (size_t i = 0; i < count; i++) {
        if (starts[i].field == TH01_START_boss_hp) {
            *hp = (int16_t)starts[i].value;
        }
    }
}
