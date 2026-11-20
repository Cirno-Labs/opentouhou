#include "retail_state.h"

#include "addresses.h"
#include "state.h"

#include "core/base/utils.h"

static uint32_t M_HashShots(const RETAIL_PLAYER *player, uint32_t *count)
{
    uint32_t hash = STATE_HASH_INIT;
    *count = 0;
    for (uint32_t i = 0; i < ARRAY_SIZE(player->bullets); i++) {
        const RETAIL_PLAYER_BULLET *const bullet = &player->bullets[i];
        if (bullet->state == 0) {
            continue;
        }
        (*count)++;
        hash = State_Hash(hash, i);
        hash = State_Hash(
            hash,
            (uint32_t)(uint16_t)bullet->state
                | (uint32_t)(uint16_t)bullet->damage << 16);
        hash = State_Hash(hash, State_F32(bullet->pos.x));
        hash = State_Hash(hash, State_F32(bullet->pos.y));
        hash = State_Hash(hash, State_F32(bullet->pos.z));
        hash = State_Hash(hash, State_F32(bullet->velocity.x));
        hash = State_Hash(hash, State_F32(bullet->velocity.y));
        hash = State_Hash(hash, State_F32(bullet->velocity.z));
    }
    return hash;
}

static uint32_t M_HashBullets(
    const RETAIL_BULLET_MANAGER *bullets, uint32_t *count)
{
    uint32_t hash = STATE_HASH_INIT;
    *count = 0;
    for (uint32_t i = 0; i < RETAIL_BULLET_COUNT; i++) {
        const RETAIL_BULLET *const bullet = &bullets->bullets[i];
        if (bullet->state == 0) {
            continue;
        }
        (*count)++;
        hash = State_Hash(hash, i);
        hash = State_Hash(
            hash, (uint32_t)bullet->state | (uint32_t)bullet->flags << 16);
        hash = State_Hash(hash, State_F32(bullet->pos.x));
        hash = State_Hash(hash, State_F32(bullet->pos.y));
        hash = State_Hash(hash, State_F32(bullet->pos.z));
        hash = State_Hash(hash, State_F32(bullet->velocity.x));
        hash = State_Hash(hash, State_F32(bullet->velocity.y));
        hash = State_Hash(hash, State_F32(bullet->velocity.z));
        hash = State_Hash(hash, State_F32(bullet->speed));
        hash = State_Hash(hash, State_F32(bullet->angle));
        hash = State_Hash(hash, (uint32_t)bullet->timer.current);
        hash = State_Hash(hash, bullet->grazed);
    }
    return hash;
}

static uint32_t M_HashLasers(
    const RETAIL_BULLET_MANAGER *bullets, uint32_t *count)
{
    uint32_t hash = STATE_HASH_INIT;
    *count = 0;
    for (uint32_t i = 0; i < ARRAY_SIZE(bullets->lasers); i++) {
        const RETAIL_LASER *const laser = &bullets->lasers[i];
        if (laser->active == 0) {
            continue;
        }
        (*count)++;
        hash = State_Hash(hash, i);
        hash = State_Hash(hash, (uint32_t)laser->state);
        hash = State_Hash(hash, State_F32(laser->pos.x));
        hash = State_Hash(hash, State_F32(laser->pos.y));
        hash = State_Hash(hash, State_F32(laser->pos.z));
        hash = State_Hash(hash, State_F32(laser->angle));
        hash = State_Hash(hash, State_F32(laser->start_offset));
        hash = State_Hash(hash, State_F32(laser->end_offset));
        hash = State_Hash(hash, (uint32_t)laser->timer.current);
    }
    return hash;
}

static uint32_t M_HashEnemies(
    const RETAIL_ENEMY_MANAGER *enemies, uint32_t *count)
{
    uint32_t hash = STATE_HASH_INIT;
    *count = 0;
    for (uint32_t i = 0; i < RETAIL_ENEMY_COUNT; i++) {
        const RETAIL_ENEMY *const enemy = &enemies->enemies[i];
        if ((enemy->flags & RETAIL_ENEMY_FLAGS_ACTIVE) == 0) {
            continue;
        }
        (*count)++;
        hash = State_Hash(hash, i);
        hash = State_Hash(hash, State_F32(enemy->pos.x));
        hash = State_Hash(hash, State_F32(enemy->pos.y));
        hash = State_Hash(hash, State_F32(enemy->pos.z));
        hash = State_Hash(hash, (uint32_t)enemy->life);
    }
    return hash;
}

static uint32_t M_HashItems(const RETAIL_ITEM *items, uint32_t *count)
{
    uint32_t hash = STATE_HASH_INIT;
    *count = 0;
    for (uint32_t i = 0; i < RETAIL_ITEM_COUNT; i++) {
        const RETAIL_ITEM *const item = &items[i];
        if (item->active == 0) {
            continue;
        }
        (*count)++;
        hash = State_Hash(hash, i);
        hash =
            State_Hash(hash, (uint32_t)item->type | (uint32_t)item->state << 8);
        hash = State_Hash(hash, State_F32(item->pos.x));
        hash = State_Hash(hash, State_F32(item->pos.y));
        hash = State_Hash(hash, State_F32(item->pos.z));
    }
    return hash;
}

void Th07Retail_Sample(void *const context, uint32_t *const row)
{
    const RETAIL_GAME_MANAGER *const game = RETAIL_GAME_MANAGER_AT;
    row[TH07_COLUMN_buttons] = RETAIL_PLAY_BUTTONS;
    row[TH07_COLUMN_stage] = (uint32_t)game->stage;
    row[TH07_COLUMN_stage_frames] = (uint32_t)game->stage_frames;
    row[TH07_COLUMN_game_speed] = State_F32(RETAIL_APP_AT->game_speed);
    // The title holds an uninitialised GameStats.
    for (int32_t i = TH07_COLUMN_score; i <= TH07_COLUMN_cherry_plus; i++) {
        row[i] = 0;
    }
    if (RETAIL_APP_AT->cur_scene == RETAIL_SCENE_GAMEPLAY) {
        const RETAIL_GAME_STATS *const stats = game->stats;
        const int32_t offset = stats->cherry_offset;
        row[TH07_COLUMN_score] = stats->score;
        row[TH07_COLUMN_gui_score] = stats->gui_score;
        row[TH07_COLUMN_lives] = State_F32(stats->lives);
        row[TH07_COLUMN_bombs] = State_F32(stats->bombs);
        row[TH07_COLUMN_power] = State_F32(stats->power);
        row[TH07_COLUMN_graze] = (uint32_t)stats->graze;
        row[TH07_COLUMN_point_items] = (uint32_t)stats->point_items;
        row[TH07_COLUMN_next_extend_items] = (uint32_t)stats->next_extend_items;
        row[TH07_COLUMN_cherry] = (uint32_t)(game->cherry - offset);
        row[TH07_COLUMN_cherry_max] = (uint32_t)(game->cherry_max - offset);
        row[TH07_COLUMN_cherry_plus] = (uint32_t)(game->cherry_plus - offset);
    }
    row[TH07_COLUMN_rank] = (uint32_t)game->rank;
    row[TH07_COLUMN_sub_rank] = (uint32_t)game->sub_rank;
    row[TH07_COLUMN_rng_seed] = RETAIL_RNG_AT->seed;
    row[TH07_COLUMN_rng_generated] = (uint32_t)RETAIL_RNG_AT->generated;

    const RETAIL_PLAYER *const player = RETAIL_PLAYER_AT;
    uint32_t shot_count;
    const uint32_t shot_hash = M_HashShots(player, &shot_count);
    row[TH07_COLUMN_player_x] = State_F32(player->position.x);
    row[TH07_COLUMN_player_y] = State_F32(player->position.y);
    row[TH07_COLUMN_player_state] = (uint8_t)player->player_state;
    row[TH07_COLUMN_border_state] = (uint8_t)player->border_state;
    row[TH07_COLUMN_shot_count] = shot_count;
    row[TH07_COLUMN_shot_hash] = shot_hash;

    const RETAIL_BULLET_MANAGER *const bullets = RETAIL_BULLET_MANAGER_AT;
    uint32_t bullet_count;
    row[TH07_COLUMN_bullet_hash] = M_HashBullets(bullets, &bullet_count);
    row[TH07_COLUMN_bullet_count] = bullet_count;
    uint32_t laser_count;
    row[TH07_COLUMN_laser_hash] = M_HashLasers(bullets, &laser_count);
    row[TH07_COLUMN_laser_count] = laser_count;

    const RETAIL_ENEMY_MANAGER *const enemies = RETAIL_ENEMY_MANAGER_AT;
    row[TH07_COLUMN_spell_card] = (uint8_t)enemies->spell_card_active
        | (uint32_t)(uint8_t)enemies->spell_card_capture << 8
        | (uint32_t)(uint16_t)enemies->spell_card_index << 16;
    row[TH07_COLUMN_spell_card_bonus] = (uint32_t)enemies->spell_card_bonus;
    uint32_t enemy_count;
    row[TH07_COLUMN_enemy_hash] = M_HashEnemies(enemies, &enemy_count);
    row[TH07_COLUMN_enemy_count] = enemy_count;

    uint32_t item_count;
    row[TH07_COLUMN_item_hash] =
        M_HashItems(RETAIL_ITEM_MANAGER_AT->items, &item_count);
    row[TH07_COLUMN_item_count] = item_count;

    row[TH07_COLUMN_difficulty] = (uint32_t)game->difficulty;
    row[TH07_COLUMN_character] = game->character;
    row[TH07_COLUMN_shot_type] = game->shot_type;
}
