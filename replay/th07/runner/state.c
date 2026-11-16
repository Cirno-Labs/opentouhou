#include "state.h"

#include "../../core/case.h"

#include "th07/bullet.h"
#include "th07/enemy.h"
#include "th07/game.h"
#include "th07/game_manager.h"
#include "th07/item.h"
#include "th07/player.h"
#include "th07/replay.h"

#include "core/base/utils.h"

static uint32_t M_HashVec3(uint32_t hash, const VEC3 vec)
{
    hash = Case_Hash(hash, Case_F32Bits(vec.x));
    hash = Case_Hash(hash, Case_F32Bits(vec.y));
    return Case_Hash(hash, Case_F32Bits(vec.z));
}

static uint32_t M_HashTimer(uint32_t hash, const TIMER *const timer)
{
    hash = Case_Hash(hash, (uint32_t)timer->current);
    return Case_Hash(hash, Case_F32Bits(timer->sub_frame));
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
        hash = M_HashVec3(hash, bullet->pos);
        hash = M_HashVec3(hash, bullet->velocity);
        hash = Case_Hash(hash, Case_F32Bits(bullet->angle));
        hash = M_HashTimer(hash, &bullet->timer);
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
        hash = Case_Hash(
            hash,
            (uint32_t)(uint16_t)bullet->running
                | (uint32_t)(uint16_t)bullet->grazed << 16);
        hash = M_HashVec3(hash, bullet->pos);
        hash = M_HashVec3(hash, bullet->velocity);
        hash = Case_Hash(hash, Case_F32Bits(bullet->speed));
        hash = Case_Hash(hash, Case_F32Bits(bullet->angle));
        hash = M_HashTimer(hash, &bullet->timer);
        hash = Case_Hash(hash, (uint32_t)bullet->next_command);
        // The five behaviours in the order the original's ex[] holds them:
        // start fast, accelerate, curve, turn, bounce. Bounce keeps no
        // timer, so its slot's timer reads 0.
        hash = Case_Hash(hash, (uint32_t)bullet->start_fast.timer.current);
        hash = Case_Hash(hash, 0);
        hash = Case_Hash(hash, (uint32_t)bullet->accelerate.timer.current);
        hash = Case_Hash(hash, 0);
        hash = Case_Hash(hash, (uint32_t)bullet->curve.timer.current);
        hash = Case_Hash(hash, 0);
        hash = Case_Hash(hash, (uint32_t)bullet->turn.timer.current);
        hash = Case_Hash(hash, (uint32_t)bullet->turn.count);
        hash = Case_Hash(hash, 0);
        hash = Case_Hash(hash, (uint32_t)bullet->bounce.count);
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
        hash = Case_Hash(
            hash, (uint32_t)laser->state | (uint32_t)laser->flags << 16);
        hash = M_HashVec3(hash, laser->pos);
        hash = Case_Hash(hash, Case_F32Bits(laser->angle));
        hash = Case_Hash(hash, Case_F32Bits(laser->start_offset));
        hash = Case_Hash(hash, Case_F32Bits(laser->end_offset));
        hash = Case_Hash(hash, Case_F32Bits(laser->width));
        hash = M_HashTimer(hash, &laser->timer);
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
        hash = M_HashVec3(hash, enemy->move.pos);
        hash = Case_Hash(hash, (uint32_t)enemy->life);
        hash = Case_Hash(hash, Case_F32Bits(enemy->move.angle));
        hash = Case_Hash(hash, Case_F32Bits(enemy->move.speed));
        hash = M_HashTimer(hash, &enemy->script.state.timer);
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
        hash = M_HashVec3(hash, item->pos);
        hash = M_HashVec3(hash, item->velocity);
        hash = M_HashTimer(hash, &item->timer);
    }
    return hash;
}

void RunnerState_Start(uint32_t row[TH07_START_COUNT])
{
    const GAME_MANAGER *const game = &g_GameManager;
    row[TH07_START_difficulty] = (uint32_t)game->run.difficulty;
    row[TH07_START_character] = game->run.character;
    row[TH07_START_shot_type] = game->run.shot_type;
    row[TH07_START_stage] = (uint32_t)game->run.stage;
    row[TH07_START_lives] = Case_F32Bits(game->player.lives);
    row[TH07_START_bombs] = Case_F32Bits(game->player.bombs);
    row[TH07_START_power] = Case_F32Bits(game->player.power);
    row[TH07_START_score] = game->score.value;
    row[TH07_START_rng_seed] = g_Rng.seed;
    row[TH07_START_rank] = (uint32_t)game->rank.value;
    row[TH07_START_sub_rank] = (uint32_t)game->rank.sub;
    row[TH07_START_cherry] =
        (uint32_t)(game->cherry.value - game->cherry.offset);
    row[TH07_START_cherry_max] =
        (uint32_t)(game->cherry.max - game->cherry.offset);
    row[TH07_START_cherry_plus] =
        (uint32_t)(game->cherry.plus - game->cherry.offset);
}

void RunnerState_Calc(uint32_t row[TH07_CALC_COUNT])
{
    const GAME_MANAGER *const game = &g_GameManager;
    // The low byte packs the original's stateFlags bits: demo at 0x02,
    // playing at 0x04, in a replay at 0x08.
    const uint32_t state_flags = (uint32_t)game->mode.in_demo << 1
        | (uint32_t)game->mode.playing << 2
        | (uint32_t)game->mode.in_replay << 3;
    const uint32_t flags = state_flags | (uint32_t)game->mode.paused << 8
        | (uint32_t)game->mode.in_continue_menu << 16;
    row[TH07_CALC_buttons] = g_Buttons.play_word;
    row[TH07_CALC_events] = Replay_GetLastEvents();
    row[TH07_CALC_stage] = (uint32_t)game->run.stage;
    row[TH07_CALC_stage_frames] = (uint32_t)game->run.stage_frames;
    row[TH07_CALC_flags] = flags;
    row[TH07_CALC_game_speed] = Case_F32Bits(g_GameSpeed);
    row[TH07_CALC_score] = game->score.value;
    row[TH07_CALC_gui_score] = game->score.shown;
    row[TH07_CALC_lives] = Case_F32Bits(game->player.lives);
    row[TH07_CALC_bombs] = Case_F32Bits(game->player.bombs);
    row[TH07_CALC_power] = Case_F32Bits(game->player.power);
    row[TH07_CALC_graze] = (uint32_t)game->player.graze;
    row[TH07_CALC_graze_total] = (uint32_t)game->player.graze_total;
    row[TH07_CALC_point_items] = (uint32_t)game->player.point_items;
    row[TH07_CALC_point_items_total] = (uint32_t)game->player.point_items_total;
    row[TH07_CALC_cherry] =
        (uint32_t)(game->cherry.value - game->cherry.offset);
    row[TH07_CALC_cherry_max] =
        (uint32_t)(game->cherry.max - game->cherry.offset);
    row[TH07_CALC_cherry_plus] =
        (uint32_t)(game->cherry.plus - game->cherry.offset);
    row[TH07_CALC_rank] = (uint32_t)game->rank.value;
    row[TH07_CALC_sub_rank] = (uint32_t)game->rank.sub;
    row[TH07_CALC_deaths] = (uint32_t)game->stats.deaths;
    row[TH07_CALC_bombs_used] = (uint32_t)game->stats.bombs_used;
    row[TH07_CALC_spell_cards_captured] =
        (uint32_t)game->stats.spell_cards_captured;
    row[TH07_CALC_rng_seed] = g_Rng.seed;
    row[TH07_CALC_rng_generated] = (uint32_t)g_Rng.generated;

    const PLAYER *const player = &g_Player;
    uint32_t shot_count;
    row[TH07_CALC_shot_hash] = M_HashShots(player, &shot_count);
    row[TH07_CALC_shot_count] = shot_count;
    row[TH07_CALC_player_x] = Case_F32Bits(player->pos.x);
    row[TH07_CALC_player_y] = Case_F32Bits(player->pos.y);
    row[TH07_CALC_player_state] = (uint8_t)player->state.value;

    const BULLET_MANAGER *const bullets = &g_Bullets;
    uint32_t bullet_count;
    row[TH07_CALC_bullet_hash] = M_HashBullets(bullets, &bullet_count);
    row[TH07_CALC_bullet_count] = bullet_count;
    uint32_t laser_count;
    row[TH07_CALC_laser_hash] = M_HashLasers(bullets, &laser_count);
    row[TH07_CALC_laser_count] = laser_count;

    const ENEMY_MANAGER *const enemies = &g_Enemies;
    row[TH07_CALC_spell_card] = (uint8_t)enemies->spell_card.state
        | (uint8_t)enemies->spell_card.capturable << 8
        | (uint32_t)(uint16_t)enemies->spell_card.index << 16;
    row[TH07_CALC_spell_card_bonus] = (uint32_t)enemies->spell_card.bonus;
    uint32_t enemy_count;
    row[TH07_CALC_enemy_hash] = M_HashEnemies(enemies, &enemy_count);
    row[TH07_CALC_enemy_count] = enemy_count;

    const ITEM *const items = Item_GetSlots();
    uint32_t item_count;
    row[TH07_CALC_item_hash] = M_HashItems(items, &item_count);
    row[TH07_CALC_item_count] = item_count;
}
