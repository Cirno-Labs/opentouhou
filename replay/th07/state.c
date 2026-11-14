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

static uint32_t M_HashVec3(uint32_t hash, const RETAIL_VEC3 *const vec)
{
    hash = Case_Hash(hash, M_F32Bits(vec->x));
    hash = Case_Hash(hash, M_F32Bits(vec->y));
    return Case_Hash(hash, M_F32Bits(vec->z));
}

static uint32_t M_HashTimer(uint32_t hash, const RETAIL_TIMER *const timer)
{
    hash = Case_Hash(hash, (uint32_t)timer->current);
    return Case_Hash(hash, M_F32Bits(timer->sub_frame));
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
        hash = M_HashVec3(hash, &bullet->pos);
        hash = M_HashVec3(hash, &bullet->velocity);
        hash = Case_Hash(hash, M_F32Bits(bullet->angle));
        hash = M_HashTimer(hash, &bullet->timer);
    }
    return hash;
}

static uint32_t M_HashBullets(
    const RETAIL_BULLET_MANAGER *bullets, uint32_t *count)
{
    uint32_t hash = CASE_HASH_INIT;
    *count = 0;
    for (uint32_t i = 0; i < ARRAY_SIZE(bullets->bullets) - 1; i++) {
        const RETAIL_BULLET *const bullet = &bullets->bullets[i];
        if (bullet->state == 0) {
            continue;
        }
        (*count)++;
        hash = Case_Hash(hash, i);
        hash = Case_Hash(
            hash, (uint32_t)bullet->state | (uint32_t)bullet->flags << 16);
        hash = Case_Hash(
            hash,
            (uint32_t)bullet->active_flags | (uint32_t)bullet->grazed << 16);
        hash = M_HashVec3(hash, &bullet->pos);
        hash = M_HashVec3(hash, &bullet->velocity);
        hash = Case_Hash(hash, M_F32Bits(bullet->speed));
        hash = Case_Hash(hash, M_F32Bits(bullet->angle));
        hash = M_HashTimer(hash, &bullet->timer);
        hash = Case_Hash(hash, (uint32_t)bullet->command_index);
        for (uint32_t j = 0; j < ARRAY_SIZE(bullet->ex); j++) {
            hash = Case_Hash(hash, (uint32_t)bullet->ex[j].timer.current);
            hash = Case_Hash(hash, (uint32_t)bullet->ex[j].turn_count);
        }
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
        hash = Case_Hash(
            hash, (uint32_t)laser->state | (uint32_t)laser->flags << 16);
        hash = M_HashVec3(hash, &laser->pos);
        hash = Case_Hash(hash, M_F32Bits(laser->angle));
        hash = Case_Hash(hash, M_F32Bits(laser->start_offset));
        hash = Case_Hash(hash, M_F32Bits(laser->end_offset));
        hash = Case_Hash(hash, M_F32Bits(laser->width));
        hash = M_HashTimer(hash, &laser->timer);
    }
    return hash;
}

static uint32_t M_HashEnemies(
    const RETAIL_ENEMY_MANAGER *enemies, uint32_t *count)
{
    uint32_t hash = CASE_HASH_INIT;
    *count = 0;
    for (uint32_t i = 0; i < ARRAY_SIZE(enemies->enemies); i++) {
        const RETAIL_ENEMY *const enemy = &enemies->enemies[i];
        if ((enemy->flags & RETAIL_ENEMY_FLAGS_ACTIVE) == 0) {
            continue;
        }
        (*count)++;
        hash = Case_Hash(hash, i);
        hash = M_HashVec3(hash, &enemy->pos);
        hash = Case_Hash(hash, (uint32_t)enemy->life);
        hash = Case_Hash(hash, M_F32Bits(enemy->angle));
        hash = Case_Hash(hash, M_F32Bits(enemy->speed));
        hash = M_HashTimer(hash, &enemy->script_timer);
    }
    return hash;
}

static uint32_t M_HashItems(const RETAIL_ITEM_MANAGER *items, uint32_t *count)
{
    uint32_t hash = CASE_HASH_INIT;
    *count = 0;
    for (uint32_t i = 0; i < ARRAY_SIZE(items->items); i++) {
        const RETAIL_ITEM *const item = &items->items[i];
        if (item->active == 0) {
            continue;
        }
        (*count)++;
        hash = Case_Hash(hash, i);
        hash =
            Case_Hash(hash, (uint32_t)item->type | (uint32_t)item->state << 8);
        hash = M_HashVec3(hash, &item->pos);
        hash = M_HashVec3(hash, &item->velocity);
        hash = M_HashTimer(hash, &item->timer);
    }
    return hash;
}

// Playback reads an input and moves the cursor past it; recording moves
// the cursor onto the input it writes. ReplayManager::frameEvents is not
// set during playback.
static uint32_t M_ReplayEvents(const RETAIL_GAME_MANAGER *const game)
{
    const RETAIL_REPLAY_MANAGER *const replay = RETAIL_REPLAY_MANAGER_PTR;
    if (replay == nullptr || replay->data == nullptr
        || replay->cursor == nullptr) {
        return 0;
    }
    const int32_t slot = MIN(game->stage - 1, 6);
    if (slot < 0 || replay->data->stages[slot] == 0) {
        return 0;
    }
    const RETAIL_REPLAY_STAGE *const stage =
        (const RETAIL_REPLAY_STAGE *)replay->data->stages[slot];
    if (replay->cursor <= stage->frames) {
        return 0;
    }
    const RETAIL_REPLAY_FRAME *const last =
        replay->mode == RETAIL_REPLAY_PLAYING ? replay->cursor - 1
                                              : replay->cursor;
    return last->events;
}

// Returns GameManager's cherry counter as a replay stage stores it.
static uint32_t M_Cherry(const RETAIL_GAME_MANAGER *const game, int32_t value)
{
    return (uint32_t)(value - game->stats->graze_count);
}

void State_Start(uint32_t row[TH07_START_COUNT])
{
    const RETAIL_GAME_MANAGER *const game = RETAIL_GAME_MANAGER_AT;
    const RETAIL_GAME_STATS *const stats = game->stats;
    row[TH07_START_difficulty] = (uint32_t)game->difficulty;
    row[TH07_START_character] = game->character;
    row[TH07_START_shot_type] = game->shot_type;
    row[TH07_START_stage] = (uint32_t)game->stage;
    row[TH07_START_lives] = M_F32Bits(stats->lives);
    row[TH07_START_bombs] = M_F32Bits(stats->bombs);
    row[TH07_START_power] = M_F32Bits(stats->power);
    row[TH07_START_score] = stats->score;
    row[TH07_START_rng_seed] = RETAIL_RNG_AT->seed;
    row[TH07_START_rank] = (uint32_t)game->rank;
    row[TH07_START_sub_rank] = (uint32_t)game->sub_rank;
    row[TH07_START_cherry] = M_Cherry(game, game->bonus_points);
    row[TH07_START_cherry_max] = M_Cherry(game, game->bonus_points_cap);
    row[TH07_START_cherry_plus] = M_Cherry(game, game->invulnerability_bonus);
}

void State_Calc(uint32_t row[TH07_CALC_COUNT])
{
    const RETAIL_GAME_MANAGER *const game = RETAIL_GAME_MANAGER_AT;
    const RETAIL_GAME_STATS *const stats = game->stats;
    const uint32_t flags = (game->state_flags & 0xff)
        | (uint32_t)game->is_paused << 8
        | (uint32_t)game->is_in_continue_menu << 16
        | (uint32_t)(uint8_t)game->is_frozen << 24;
    row[TH07_CALC_buttons] = RETAIL_PLAY_BUTTONS;
    row[TH07_CALC_events] = M_ReplayEvents(game);
    row[TH07_CALC_stage] = (uint32_t)game->stage;
    row[TH07_CALC_stage_frames] = (uint32_t)game->stage_frames;
    row[TH07_CALC_flags] = flags;
    row[TH07_CALC_game_speed] = M_F32Bits(RETAIL_APP_AT->game_speed);
    row[TH07_CALC_score] = stats->score;
    row[TH07_CALC_gui_score] = stats->gui_score;
    row[TH07_CALC_lives] = M_F32Bits(stats->lives);
    row[TH07_CALC_bombs] = M_F32Bits(stats->bombs);
    row[TH07_CALC_power] = M_F32Bits(stats->power);
    row[TH07_CALC_graze] = (uint32_t)stats->graze;
    row[TH07_CALC_graze_total] = (uint32_t)stats->graze_total;
    row[TH07_CALC_point_items] = (uint32_t)stats->point_items;
    row[TH07_CALC_point_items_total] = (uint32_t)stats->point_items_total;
    row[TH07_CALC_cherry] = M_Cherry(game, game->bonus_points);
    row[TH07_CALC_cherry_max] = M_Cherry(game, game->bonus_points_cap);
    row[TH07_CALC_cherry_plus] = M_Cherry(game, game->invulnerability_bonus);
    row[TH07_CALC_rank] = (uint32_t)game->rank;
    row[TH07_CALC_sub_rank] = (uint32_t)game->sub_rank;
    row[TH07_CALC_deaths] = (uint32_t)stats->deaths;
    row[TH07_CALC_bombs_used] = (uint32_t)stats->bombs_used;
    row[TH07_CALC_spell_cards_captured] = (uint32_t)stats->spell_cards_captured;
    row[TH07_CALC_rng_seed] = RETAIL_RNG_AT->seed;
    row[TH07_CALC_rng_generated] = RETAIL_RNG_AT->generated;

    const RETAIL_PLAYER *const player = RETAIL_PLAYER_AT;
    uint32_t shot_count;
    row[TH07_CALC_shot_hash] = M_HashShots(player, &shot_count);
    row[TH07_CALC_shot_count] = shot_count;
    row[TH07_CALC_player_x] = M_F32Bits(player->position.x);
    row[TH07_CALC_player_y] = M_F32Bits(player->position.y);
    row[TH07_CALC_player_state] = (uint8_t)player->player_state;

    const RETAIL_BULLET_MANAGER *const bullets = RETAIL_BULLET_MANAGER_AT;
    uint32_t bullet_count;
    row[TH07_CALC_bullet_hash] = M_HashBullets(bullets, &bullet_count);
    row[TH07_CALC_bullet_count] = bullet_count;
    uint32_t laser_count;
    row[TH07_CALC_laser_hash] = M_HashLasers(bullets, &laser_count);
    row[TH07_CALC_laser_count] = laser_count;

    const RETAIL_ENEMY_MANAGER *const enemies = RETAIL_ENEMY_MANAGER_AT;
    row[TH07_CALC_spell_card] = (uint8_t)enemies->spell_card_active
        | (uint8_t)enemies->spell_card_capture << 8
        | (uint16_t)enemies->spell_card_index << 16;
    row[TH07_CALC_spell_card_bonus] = (uint32_t)enemies->spell_card_bonus;
    uint32_t enemy_count;
    row[TH07_CALC_enemy_hash] = M_HashEnemies(enemies, &enemy_count);
    row[TH07_CALC_enemy_count] = enemy_count;

    const RETAIL_ITEM_MANAGER *const items = RETAIL_ITEM_MANAGER_AT;
    uint32_t item_count;
    row[TH07_CALC_item_hash] = M_HashItems(items, &item_count);
    row[TH07_CALC_item_count] = item_count;
}
