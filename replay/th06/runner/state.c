#include "state.h"

#include "../../core/case.h"

#include "th06/bullet.h"
#include "th06/enemy.h"
#include "th06/game.h"
#include "th06/game_manager.h"
#include "th06/item.h"
#include "th06/player.h"

#include "core/base/utils.h"

#include <string.h>

static uint32_t M_F32Bits(float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static uint32_t M_HashShots(const PLAYER *player, uint32_t *count)
{
    uint32_t hash = CASE_HASH_INIT;
    *count = 0;
    for (uint32_t i = 0; i < ARRAY_SIZE(player->shots.bullets); i++) {
        const PLAYER_BULLET *const bullet = &player->shots.bullets[i];
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

static uint32_t M_HashBullets(const BULLET_MANAGER *bullets, uint32_t *count)
{
    uint32_t hash = CASE_HASH_INIT;
    *count = 0;
    for (uint32_t i = 0; i < ARRAY_SIZE(bullets->bullets); i++) {
        const BULLET *const bullet = &bullets->bullets[i];
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
        hash = Case_Hash(hash, (uint32_t)bullet->timer.current);
        hash = Case_Hash(hash, bullet->grazed);
    }
    return hash;
}

static uint32_t M_HashLasers(const BULLET_MANAGER *bullets, uint32_t *count)
{
    uint32_t hash = CASE_HASH_INIT;
    *count = 0;
    for (uint32_t i = 0; i < ARRAY_SIZE(bullets->lasers); i++) {
        const LASER *const laser = &bullets->lasers[i];
        if (!laser->active) {
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
        hash = Case_Hash(hash, (uint32_t)laser->timer.current);
    }
    return hash;
}

static uint32_t M_HashEnemies(const ENEMY_MANAGER *enemies, uint32_t *count)
{
    uint32_t hash = CASE_HASH_INIT;
    *count = 0;
    for (uint32_t i = 0; i < ARRAY_SIZE(enemies->enemies); i++) {
        const ENEMY *const enemy = &enemies->enemies[i];
        if (!enemy->flags.active) {
            continue;
        }
        (*count)++;
        hash = Case_Hash(hash, i);
        hash = Case_Hash(hash, M_F32Bits(enemy->move.pos.x));
        hash = Case_Hash(hash, M_F32Bits(enemy->move.pos.y));
        hash = Case_Hash(hash, M_F32Bits(enemy->move.pos.z));
        hash = Case_Hash(hash, (uint32_t)enemy->life);
    }
    return hash;
}

static uint32_t M_HashItems(const ITEM *items, uint32_t *count)
{
    uint32_t hash = CASE_HASH_INIT;
    *count = 0;
    for (uint32_t i = 0; i < ITEM_COUNT; i++) {
        const ITEM *const item = &items[i];
        if (!item->active) {
            continue;
        }
        (*count)++;
        hash = Case_Hash(hash, i);
        hash = Case_Hash(
            hash,
            (uint32_t)(uint8_t)item->type
                | (uint32_t)(uint8_t)item->state << 8);
        hash = Case_Hash(hash, M_F32Bits(item->pos.x));
        hash = Case_Hash(hash, M_F32Bits(item->pos.y));
        hash = Case_Hash(hash, M_F32Bits(item->pos.z));
    }
    return hash;
}

void RunnerState_Start(uint32_t row[TH06_START_COUNT])
{
    const GAME_MANAGER *const game = &g_GameManager;
    row[TH06_START_difficulty] = (uint32_t)game->run.difficulty;
    row[TH06_START_character] = game->run.character;
    row[TH06_START_shot_type] = game->run.shot_type;
    row[TH06_START_stage] = (uint32_t)game->run.stage;
    row[TH06_START_lives] = (uint32_t)game->player.lives;
    row[TH06_START_bombs] = (uint32_t)game->player.bombs;
    row[TH06_START_power] = game->player.power;
    row[TH06_START_score] = game->score.value;
    row[TH06_START_rng_seed] = g_Rng.seed;
    row[TH06_START_rank] = (uint32_t)game->rank.value;
    row[TH06_START_sub_rank] = (uint32_t)game->rank.sub;
    row[TH06_START_power_item_combo] = (uint8_t)game->player.power_item_combo;
}

void RunnerState_Calc(uint32_t row[TH06_CALC_COUNT])
{
    const GAME_MANAGER *const game = &g_GameManager;
    const uint32_t flags = game->mode.playing | (uint32_t)game->mode.paused << 8
        | (uint32_t)game->mode.in_continue_menu << 16
        | (uint32_t)(uint8_t)game->mode.frozen << 24;
    row[TH06_CALC_buttons] = g_Buttons.word;
    row[TH06_CALC_stage] = (uint32_t)game->run.stage;
    row[TH06_CALC_stage_frames] = (uint32_t)game->run.stage_frames;
    row[TH06_CALC_flags] = flags;
    row[TH06_CALC_score] = game->score.value;
    row[TH06_CALC_gui_score] = game->score.shown;
    row[TH06_CALC_lives] = (uint32_t)game->player.lives;
    row[TH06_CALC_bombs] = (uint32_t)game->player.bombs;
    row[TH06_CALC_power] = game->player.power;
    row[TH06_CALC_graze] = (uint32_t)game->player.graze;
    row[TH06_CALC_point_items] = game->player.point_items;
    row[TH06_CALC_rank] = (uint32_t)game->rank.value;
    row[TH06_CALC_sub_rank] = (uint32_t)game->rank.sub;
    row[TH06_CALC_next_extend] = (uint8_t)game->score.next_extend;
    row[TH06_CALC_rng_seed] = g_Rng.seed;
    row[TH06_CALC_rng_generated] = (uint32_t)g_Rng.generated;

    const PLAYER *const player = &g_Player;
    uint32_t shot_count;
    const uint32_t shot_hash = M_HashShots(player, &shot_count);
    row[TH06_CALC_player_x] = M_F32Bits(player->pos.x);
    row[TH06_CALC_player_y] = M_F32Bits(player->pos.y);
    row[TH06_CALC_player_state] = (uint8_t)player->state.value;
    row[TH06_CALC_shot_count] = shot_count;
    row[TH06_CALC_shot_hash] = shot_hash;

    const BULLET_MANAGER *const bullets = &g_Bullets;
    uint32_t bullet_count;
    row[TH06_CALC_bullet_hash] = M_HashBullets(bullets, &bullet_count);
    row[TH06_CALC_bullet_count] = bullet_count;
    uint32_t laser_count;
    row[TH06_CALC_laser_hash] = M_HashLasers(bullets, &laser_count);
    row[TH06_CALC_laser_count] = laser_count;

    const ENEMY_MANAGER *const enemies = &g_Enemies;
    row[TH06_CALC_spell_card] = (uint8_t)enemies->spell_card.state
        | (uint8_t)enemies->spell_card.capturable << 8
        | (uint32_t)(uint16_t)enemies->spell_card.index << 16;
    row[TH06_CALC_spell_card_bonus] = (uint32_t)enemies->spell_card.bonus;
    uint32_t enemy_count;
    row[TH06_CALC_enemy_hash] = M_HashEnemies(enemies, &enemy_count);
    row[TH06_CALC_enemy_count] = enemy_count;

    const ITEM *const items = Item_GetSlots();
    uint32_t item_count;
    row[TH06_CALC_item_hash] = M_HashItems(items, &item_count);
    row[TH06_CALC_item_count] = item_count;
}
