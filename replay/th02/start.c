#include "start.h"

#include "harness.h"

void Th02Start_Apply(
    const START_VALUE *const starts, const size_t count,
    TH02_SESSION *const session)
{
    for (size_t i = 0; i < count; i++) {
        const uint32_t value = starts[i].value;
        switch (starts[i].field) {
        case TH02_START_stage:
            session->stage = (uint8_t)value;
            break;
        case TH02_START_shot_type:
            session->shot_type = (uint8_t)value;
            break;
        case TH02_START_rank:
            session->rank = (int8_t)value;
            break;
        case TH02_START_score:
            session->score = (int32_t)value;
            break;
        case TH02_START_continues:
            session->continues_used = (uint16_t)value;
            break;
        case TH02_START_lives:
            session->lives = (int8_t)value;
            session->start_lives = (uint8_t)value;
            break;
        case TH02_START_bombs:
            session->bombs = (int8_t)value;
            session->start_bombs = (uint8_t)value;
            break;
        case TH02_START_power:
            session->start_power = (int8_t)value;
            break;
        case TH02_START_skill:
            session->skill = (int16_t)value;
            break;
        case TH02_START_boss_hp:
            break;
        }
    }
}

void Th02Start_ApplyBoss(
    const START_VALUE *const starts, const size_t count, int16_t *const damage,
    const int16_t threshold)
{
    for (size_t i = 0; i < count; i++) {
        if (starts[i].field == TH02_START_boss_hp) {
            *damage =
                (int16_t)((uint16_t)threshold - (uint16_t)starts[i].value);
        }
    }
}
