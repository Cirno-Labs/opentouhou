#include "replay/th06/retail_state.h"

#include "replay/harness/state.h"
#include "replay/th06/addresses.h"

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
    }
    return hash;
}

static uint32_t M_HashBullets(
    const RETAIL_BULLET_MANAGER *bullets, uint32_t *count)
{
    uint32_t hash = STATE_HASH_INIT;
    *count = 0;
    for (uint32_t i = 0; i < ARRAY_SIZE(bullets->bullets); i++) {
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
        hash = State_Hash(hash, (uint32_t)bullet->timer_current);
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
        hash = State_Hash(hash, (uint32_t)laser->timer_current);
    }
    return hash;
}

static uint32_t M_HashEnemies(
    const RETAIL_ENEMY_MANAGER *enemies, uint32_t *count)
{
    uint32_t hash = STATE_HASH_INIT;
    *count = 0;
    for (uint32_t i = 0; i < ARRAY_SIZE(enemies->enemies); i++) {
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

static uint32_t M_HashItems(const RETAIL_ITEM_MANAGER *items, uint32_t *count)
{
    uint32_t hash = STATE_HASH_INIT;
    *count = 0;
    for (uint32_t i = 0; i < ARRAY_SIZE(items->items); i++) {
        const RETAIL_ITEM *const item = &items->items[i];
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

void Th06Retail_Sample(void *const context, uint32_t *const row)
{
    const RETAIL_GAME_MANAGER *const game = RETAIL_GAME_MANAGER_AT;
    const uint32_t flags = game->is_playing | (uint32_t)game->is_paused << 8
        | (uint32_t)game->is_in_continue_menu << 16
        | (uint32_t)(uint8_t)game->is_frozen << 24;
    row[TH06_COLUMN_buttons] = RETAIL_BUTTONS;
    row[TH06_COLUMN_stage] = (uint32_t)game->stage;
    row[TH06_COLUMN_stage_frames] = (uint32_t)game->stage_frames;
    row[TH06_COLUMN_flags] = flags;
    row[TH06_COLUMN_score] = game->score;
    row[TH06_COLUMN_gui_score] = game->gui_score;
    row[TH06_COLUMN_lives] = (uint32_t)game->lives;
    row[TH06_COLUMN_bombs] = (uint32_t)game->bombs;
    row[TH06_COLUMN_power] = game->power;
    row[TH06_COLUMN_graze] = (uint32_t)game->graze;
    row[TH06_COLUMN_point_items] = game->point_items;
    row[TH06_COLUMN_rank] = (uint32_t)game->rank;
    row[TH06_COLUMN_sub_rank] = (uint32_t)game->sub_rank;
    row[TH06_COLUMN_next_extend] = (uint8_t)game->next_extend;
    row[TH06_COLUMN_rng_seed] = RETAIL_RNG_AT->seed;
    row[TH06_COLUMN_rng_generated] = (uint32_t)RETAIL_RNG_AT->generated;

    const RETAIL_PLAYER *const player = RETAIL_PLAYER_AT;
    uint32_t shot_count;
    const uint32_t shot_hash = M_HashShots(player, &shot_count);
    row[TH06_COLUMN_player_x] = State_F32(player->position.x);
    row[TH06_COLUMN_player_y] = State_F32(player->position.y);
    row[TH06_COLUMN_player_state] = (uint8_t)player->player_state;
    row[TH06_COLUMN_shot_count] = shot_count;
    row[TH06_COLUMN_shot_hash] = shot_hash;

    const RETAIL_BULLET_MANAGER *const bullets = RETAIL_BULLET_MANAGER_AT;
    uint32_t bullet_count;
    row[TH06_COLUMN_bullet_hash] = M_HashBullets(bullets, &bullet_count);
    row[TH06_COLUMN_bullet_count] = bullet_count;
    uint32_t laser_count;
    row[TH06_COLUMN_laser_hash] = M_HashLasers(bullets, &laser_count);
    row[TH06_COLUMN_laser_count] = laser_count;

    const RETAIL_ENEMY_MANAGER *const enemies = RETAIL_ENEMY_MANAGER_AT;
    row[TH06_COLUMN_spell_card] = (uint8_t)enemies->spell_card_active
        | (uint8_t)enemies->spell_card_capture << 8
        | (uint16_t)enemies->spell_card_index << 16;
    row[TH06_COLUMN_spell_card_bonus] = (uint32_t)enemies->spell_card_bonus;
    uint32_t enemy_count;
    row[TH06_COLUMN_enemy_hash] = M_HashEnemies(enemies, &enemy_count);
    row[TH06_COLUMN_enemy_count] = enemy_count;

    const RETAIL_ITEM_MANAGER *const items = RETAIL_ITEM_MANAGER_AT;
    uint32_t item_count;
    row[TH06_COLUMN_item_hash] = M_HashItems(items, &item_count);
    row[TH06_COLUMN_item_count] = item_count;

    row[TH06_COLUMN_difficulty] = (uint32_t)game->difficulty;
    row[TH06_COLUMN_character] = game->character;
    row[TH06_COLUMN_shot_type] = game->shot_type;
}
