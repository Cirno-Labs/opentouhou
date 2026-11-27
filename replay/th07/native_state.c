#include "replay/th07/native_state.h"

#include "replay/harness/state.h"
#include "th07/bullet.h"
#include "th07/enemy.h"
#include "th07/game.h"
#include "th07/game_manager.h"
#include "th07/item.h"
#include "th07/player.h"
#include "th07/scene.h"

#include "core/base/utils.h"

static uint32_t M_HashShots(const PLAYER *player, uint32_t *count)
{
    uint32_t hash = STATE_HASH_INIT;
    *count = 0;
    for (uint32_t i = 0; i < ARRAY_SIZE(player->shots.bullets); i++) {
        const PLAYER_BULLET *const bullet = &player->shots.bullets[i];
        if (bullet->state == PLAYER_BULLET_FREE) {
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

static uint32_t M_HashBullets(const BULLET_MANAGER *bullets, uint32_t *count)
{
    uint32_t hash = STATE_HASH_INIT;
    *count = 0;
    for (uint32_t i = 0; i < ARRAY_SIZE(bullets->bullets); i++) {
        const BULLET *const bullet = &bullets->bullets[i];
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

static uint32_t M_HashLasers(const BULLET_MANAGER *bullets, uint32_t *count)
{
    uint32_t hash = STATE_HASH_INIT;
    *count = 0;
    for (uint32_t i = 0; i < ARRAY_SIZE(bullets->lasers); i++) {
        const LASER *const laser = &bullets->lasers[i];
        if (!laser->active) {
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

static uint32_t M_HashEnemies(const ENEMY_MANAGER *enemies, uint32_t *count)
{
    uint32_t hash = STATE_HASH_INIT;
    *count = 0;
    for (uint32_t i = 0; i < ARRAY_SIZE(enemies->enemies); i++) {
        const ENEMY *const enemy = &enemies->enemies[i];
        if (!enemy->flags.active) {
            continue;
        }
        (*count)++;
        hash = State_Hash(hash, i);
        hash = State_Hash(hash, State_F32(enemy->move.pos.x));
        hash = State_Hash(hash, State_F32(enemy->move.pos.y));
        hash = State_Hash(hash, State_F32(enemy->move.pos.z));
        hash = State_Hash(hash, (uint32_t)enemy->life);
    }
    return hash;
}

static uint32_t M_HashItems(const ITEM *items, uint32_t *count)
{
    uint32_t hash = STATE_HASH_INIT;
    *count = 0;
    for (uint32_t i = 0; i < ITEM_COUNT; i++) {
        const ITEM *const item = &items[i];
        if (!item->active) {
            continue;
        }
        (*count)++;
        hash = State_Hash(hash, i);
        hash = State_Hash(
            hash,
            (uint32_t)(uint8_t)item->type
                | (uint32_t)(uint8_t)item->state << 8);
        hash = State_Hash(hash, State_F32(item->pos.x));
        hash = State_Hash(hash, State_F32(item->pos.y));
        hash = State_Hash(hash, State_F32(item->pos.z));
    }
    return hash;
}

void Th07Native_Sample(void *const context, uint32_t *const row)
{
    const GAME_MANAGER *const game = &g_GameManager;
    row[TH07_COLUMN_buttons] = g_Buttons.play_word;
    row[TH07_COLUMN_stage] = (uint32_t)game->run.stage;
    row[TH07_COLUMN_stage_frames] = (uint32_t)game->run.stage_frames;
    row[TH07_COLUMN_game_speed] = State_F32(g_GameSpeed);
    // The original's title holds an uninitialized GameStats.
    for (int32_t i = TH07_COLUMN_score; i <= TH07_COLUMN_cherry_plus; i++) {
        row[i] = 0;
    }
    if (Scene_Get() == SCENE_GAMEPLAY) {
        const int32_t offset = game->cherry.offset;
        row[TH07_COLUMN_score] = game->score.value;
        row[TH07_COLUMN_gui_score] = game->score.shown;
        row[TH07_COLUMN_lives] = State_F32(game->player.lives);
        row[TH07_COLUMN_bombs] = State_F32(game->player.bombs);
        row[TH07_COLUMN_power] = State_F32(game->player.power);
        row[TH07_COLUMN_graze] = (uint32_t)game->player.graze;
        row[TH07_COLUMN_point_items] = (uint32_t)game->player.point_items;
        row[TH07_COLUMN_next_extend_items] =
            (uint32_t)game->player.next_extend_items;
        row[TH07_COLUMN_cherry] = (uint32_t)(game->cherry.value - offset);
        row[TH07_COLUMN_cherry_max] = (uint32_t)(game->cherry.max - offset);
        row[TH07_COLUMN_cherry_plus] = (uint32_t)(game->cherry.plus - offset);
    }
    row[TH07_COLUMN_rank] = (uint32_t)game->rank.value;
    row[TH07_COLUMN_sub_rank] = (uint32_t)game->rank.sub;
    row[TH07_COLUMN_rng_seed] = g_Rng.seed;
    row[TH07_COLUMN_rng_generated] = (uint32_t)g_Rng.generated;

    const PLAYER *const player = &g_Player;
    uint32_t shot_count;
    const uint32_t shot_hash = M_HashShots(player, &shot_count);
    row[TH07_COLUMN_player_x] = State_F32(player->pos.x);
    row[TH07_COLUMN_player_y] = State_F32(player->pos.y);
    row[TH07_COLUMN_player_state] = (uint8_t)player->state.value;
    row[TH07_COLUMN_border_state] = (uint8_t)player->border_state;
    row[TH07_COLUMN_shot_count] = shot_count;
    row[TH07_COLUMN_shot_hash] = shot_hash;

    const BULLET_MANAGER *const bullets = &g_Bullets;
    uint32_t bullet_count;
    row[TH07_COLUMN_bullet_hash] = M_HashBullets(bullets, &bullet_count);
    row[TH07_COLUMN_bullet_count] = bullet_count;
    uint32_t laser_count;
    row[TH07_COLUMN_laser_hash] = M_HashLasers(bullets, &laser_count);
    row[TH07_COLUMN_laser_count] = laser_count;

    const ENEMY_MANAGER *const enemies = &g_Enemies;
    row[TH07_COLUMN_spell_card] = (uint8_t)enemies->spell_card.state
        | (uint32_t)(uint8_t)enemies->spell_card.capturable << 8
        | (uint32_t)(uint16_t)enemies->spell_card.index << 16;
    row[TH07_COLUMN_spell_card_bonus] = (uint32_t)enemies->spell_card.bonus;
    uint32_t enemy_count;
    row[TH07_COLUMN_enemy_hash] = M_HashEnemies(enemies, &enemy_count);
    row[TH07_COLUMN_enemy_count] = enemy_count;

    uint32_t item_count;
    row[TH07_COLUMN_item_hash] = M_HashItems(Item_GetSlots(), &item_count);
    row[TH07_COLUMN_item_count] = item_count;

    row[TH07_COLUMN_difficulty] = (uint32_t)game->run.difficulty;
    row[TH07_COLUMN_character] = game->run.character;
    row[TH07_COLUMN_shot_type] = game->run.shot_type;
}
