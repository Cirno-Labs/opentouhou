#include "state.h"

#include "../core/case.h"
#include "retail.h"

#include "core/base/utils.h"

#include <string.h>

static uint32_t M_F32Bits(float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static uint32_t M_HashShots(const RETAIL_PLAYER *player, uint32_t *count)
{
    uint32_t hash = CASE_HASH_INIT;
    *count = 0;
    for (uint32_t i = 0; i < ARRAY_SIZE(player->bullets); i++) {
        const RETAIL_PLAYER_BULLET *const bullet = &player->bullets[i];
        if (bullet->state == 0) {
            continue;
        }
        (*count)++;
        hash = Case_Hash(hash, i);
        hash = Case_Hash(
            hash,
            (uint32_t)(uint16_t)bullet->state
                | (uint32_t)(uint16_t)bullet->damage << 16);
        hash = Case_Hash(hash, M_F32Bits(bullet->pos.x));
        hash = Case_Hash(hash, M_F32Bits(bullet->pos.y));
        hash = Case_Hash(hash, M_F32Bits(bullet->pos.z));
    }
    return hash;
}

static uint32_t M_HashBullets(
    const RETAIL_BULLET_MANAGER *bullets, uint32_t *count)
{
    uint32_t hash = CASE_HASH_INIT;
    *count = 0;
    for (uint32_t i = 0; i < ARRAY_SIZE(bullets->bullets); i++) {
        const RETAIL_BULLET *const bullet = &bullets->bullets[i];
        if (bullet->state == 0) {
            continue;
        }
        (*count)++;
        hash = Case_Hash(hash, i);
        hash = Case_Hash(
            hash, (uint32_t)bullet->state | (uint32_t)bullet->flags << 16);
        hash = Case_Hash(hash, M_F32Bits(bullet->pos.x));
        hash = Case_Hash(hash, M_F32Bits(bullet->pos.y));
        hash = Case_Hash(hash, M_F32Bits(bullet->pos.z));
        hash = Case_Hash(hash, M_F32Bits(bullet->velocity.x));
        hash = Case_Hash(hash, M_F32Bits(bullet->velocity.y));
        hash = Case_Hash(hash, M_F32Bits(bullet->velocity.z));
        hash = Case_Hash(hash, M_F32Bits(bullet->speed));
        hash = Case_Hash(hash, M_F32Bits(bullet->angle));
        hash = Case_Hash(hash, (uint32_t)bullet->timer_current);
        hash = Case_Hash(hash, bullet->grazed);
    }
    return hash;
}

static uint32_t M_HashLasers(
    const RETAIL_BULLET_MANAGER *bullets, uint32_t *count)
{
    uint32_t hash = CASE_HASH_INIT;
    *count = 0;
    for (uint32_t i = 0; i < ARRAY_SIZE(bullets->lasers); i++) {
        const RETAIL_LASER *const laser = &bullets->lasers[i];
        if (laser->active == 0) {
            continue;
        }
        (*count)++;
        hash = Case_Hash(hash, i);
        hash = Case_Hash(hash, (uint32_t)laser->state);
        hash = Case_Hash(hash, M_F32Bits(laser->pos.x));
        hash = Case_Hash(hash, M_F32Bits(laser->pos.y));
        hash = Case_Hash(hash, M_F32Bits(laser->pos.z));
        hash = Case_Hash(hash, M_F32Bits(laser->angle));
        hash = Case_Hash(hash, M_F32Bits(laser->start_offset));
        hash = Case_Hash(hash, M_F32Bits(laser->end_offset));
        hash = Case_Hash(hash, (uint32_t)laser->timer_current);
    }
    return hash;
}

void State_Start(uint32_t row[TH06_START_COUNT])
{
    const RETAIL_GAME_MANAGER *const game = RETAIL_GAME_MANAGER_AT;
    row[TH06_START_difficulty] = (uint32_t)game->difficulty;
    row[TH06_START_character] = game->character;
    row[TH06_START_shot_type] = game->shot_type;
    row[TH06_START_stage] = (uint32_t)game->stage;
    row[TH06_START_lives] = (uint32_t)game->lives;
    row[TH06_START_bombs] = (uint32_t)game->bombs;
    row[TH06_START_power] = game->power;
    row[TH06_START_score] = game->score;
    row[TH06_START_rng_seed] = RETAIL_RNG_AT->seed;
    row[TH06_START_rank] = (uint32_t)game->rank;
    row[TH06_START_sub_rank] = (uint32_t)game->sub_rank;
    row[TH06_START_power_item_combo] = (uint8_t)game->power_item_combo;
}

void State_Calc(uint32_t row[TH06_CALC_COUNT])
{
    const RETAIL_GAME_MANAGER *const game = RETAIL_GAME_MANAGER_AT;
    const uint32_t flags = game->is_playing | (uint32_t)game->is_paused << 8
        | (uint32_t)game->is_in_continue_menu << 16
        | (uint32_t)(uint8_t)game->is_frozen << 24;
    row[TH06_CALC_buttons] = RETAIL_BUTTONS;
    row[TH06_CALC_stage] = (uint32_t)game->stage;
    row[TH06_CALC_stage_frames] = (uint32_t)game->stage_frames;
    row[TH06_CALC_flags] = flags;
    row[TH06_CALC_score] = game->score;
    row[TH06_CALC_gui_score] = game->gui_score;
    row[TH06_CALC_lives] = (uint32_t)game->lives;
    row[TH06_CALC_bombs] = (uint32_t)game->bombs;
    row[TH06_CALC_power] = game->power;
    row[TH06_CALC_graze] = (uint32_t)game->graze;
    row[TH06_CALC_point_items] = game->point_items;
    row[TH06_CALC_rank] = (uint32_t)game->rank;
    row[TH06_CALC_sub_rank] = (uint32_t)game->sub_rank;
    row[TH06_CALC_next_extend] = (uint8_t)game->next_extend;
    row[TH06_CALC_rng_seed] = RETAIL_RNG_AT->seed;
    row[TH06_CALC_rng_generated] = (uint32_t)RETAIL_RNG_AT->generated;

    const RETAIL_PLAYER *const player = RETAIL_PLAYER_AT;
    uint32_t shot_count;
    const uint32_t shot_hash = M_HashShots(player, &shot_count);
    row[TH06_CALC_player_x] = M_F32Bits(player->position.x);
    row[TH06_CALC_player_y] = M_F32Bits(player->position.y);
    row[TH06_CALC_player_state] = (uint8_t)player->player_state;
    row[TH06_CALC_shot_count] = shot_count;
    row[TH06_CALC_shot_hash] = shot_hash;

    const RETAIL_BULLET_MANAGER *const bullets = RETAIL_BULLET_MANAGER_AT;
    uint32_t bullet_count;
    row[TH06_CALC_bullet_hash] = M_HashBullets(bullets, &bullet_count);
    row[TH06_CALC_bullet_count] = bullet_count;
    uint32_t laser_count;
    row[TH06_CALC_laser_hash] = M_HashLasers(bullets, &laser_count);
    row[TH06_CALC_laser_count] = laser_count;
}
