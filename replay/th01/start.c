#include "replay/th01/start.h"

#include "replay/th01/harness.h"
#include "th01/session_private.h"

#include "core/base/utils.h"

void Th01Start_Apply(const START_VALUE *const starts, const size_t count)
{
    bool scene_set = false;
    uint16_t scene_continues = 0;
    for (size_t i = 0; i < count; i++) {
        const uint32_t value = starts[i].value;
        switch (starts[i].field) {
        case TH01_START_stage:
            p_Session->stage_id = (int16_t)value;
            break;
        case TH01_START_route:
            p_Session->route = (uint8_t)value;
            break;
        case TH01_START_difficulty:
            p_Session->settings.difficulty = (int32_t)value;
            break;
        case TH01_START_lives:
            p_Session->lives = (int16_t)value;
            break;
        case TH01_START_bombs:
            p_Session->bombs = (int16_t)value;
            break;
        case TH01_START_score:
            p_Session->score = value;
            break;
        case TH01_START_continues:
            p_Session->continues_total = (int32_t)value;
            break;
        case TH01_START_pellet_speed:
            p_Session->pellet_speed = (int16_t)value;
            break;
        case TH01_START_boss_hp:
        case TH01_START_stage_time:
            break;
        case TH01_START_scene_continues:
            scene_set = true;
            scene_continues = (uint16_t)value;
            break;
        }
    }
    const size_t scene = (size_t)p_Session->stage_id / 5;
    if (scene_set && scene < ARRAY_SIZE(p_Session->continues_per_scene)) {
        p_Session->continues_per_scene[scene] = scene_continues;
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

void Th01Start_ApplyStageTime(
    const START_VALUE *const starts, const size_t count, uint16_t *const time)
{
    for (size_t i = 0; i < count; i++) {
        if (starts[i].field == TH01_START_stage_time) {
            *time = (uint16_t)starts[i].value;
        }
    }
}
