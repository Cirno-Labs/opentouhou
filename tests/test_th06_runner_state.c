// SPDX-License-Identifier: GPL-3.0-only

#include "../replay/th06/runner/state.h"

#include "test.h"

#include "th06/ascii.h"
#include "th06/bullet.h"
#include "th06/enemy.h"
#include "th06/game.h"
#include "th06/game_manager.h"
#include "th06/gui.h"
#include "th06/item.h"
#include "th06/player.h"

#include <string.h>

static_assert(_Generic(Item_GetSlots(), const ITEM *: 1, default: 0));

GAME_MANAGER g_GameManager;
PLAYER g_Player;
BULLET_MANAGER g_Bullets;
ENEMY_MANAGER g_Enemies;
RNG g_Rng;
BUTTONS g_Buttons;

static void M_Reset(void)
{
    g_GameManager = (GAME_MANAGER) {};
    g_Player = (PLAYER) {};
    g_Bullets = (BULLET_MANAGER) {};
    g_Enemies = (ENEMY_MANAGER) {};
    g_Rng = (RNG) {};
    g_Buttons = (BUTTONS) {};
    Item_Reset();
}

void Anm_SetSprite(ANM_VM *const vm, const int32_t sprite)
{
    vm->sprite = (int16_t)sprite;
}

void Anm_StartScript(ANM_VM *const vm, const int32_t script)
{
    vm->script_index = (int16_t)script;
}

bool Anm_Step(ANM_VM *const vm)
{
    return false;
}

void AsciiText_AddPopup(
    const VEC3 pos, const int32_t value, const uint32_t color)
{
}

void AsciiText_AddPopup2(
    const VEC3 pos, const int32_t value, const uint32_t color)
{
}

void Bullet_ClearAllToItems(void)
{
}

void Gui_ShowFullPowerPopup(void)
{
}

float Player_AngleFrom(const VEC3 pos)
{
    return 0.0f;
}

bool Player_TouchesGrazeBox(const VEC3 pos, const VEC3 size)
{
    return false;
}

void GameManager_AddRank(const int32_t amount)
{
}

void GameManager_SubRank(const int32_t amount)
{
}

TEST(start_checkpoint_preserves_signed_and_byte_fields)
{
    M_Reset();
    g_GameManager.run.difficulty = 4;
    g_GameManager.run.character = 1;
    g_GameManager.run.shot_type = 1;
    g_GameManager.run.stage = 7;
    g_GameManager.player.lives = -1;
    g_GameManager.player.bombs = -2;
    g_GameManager.player.power = 128;
    g_GameManager.score.value = 123456780;
    g_GameManager.rank.value = 19;
    g_GameManager.rank.sub = -7;
    g_GameManager.player.power_item_combo = -1;
    g_Rng.seed = 0xabcd;
    uint32_t row[TH06_START_COUNT];
    RunnerState_Start(row);
    const uint32_t expected[] = {
        4,   1,         1,      7,  0xffffffffu, 0xfffffffeu,
        128, 123456780, 0xabcd, 19, 0xfffffff9u, 255,
    };
    CHECK_EQ_INT(sizeof(expected), sizeof(row));
    CHECK(memcmp(row, expected, sizeof(row)) == 0);
}

TEST(calc_preserves_flags_spell_state_and_float_bits)
{
    M_Reset();
    g_Buttons.word = 0x1a5;
    g_GameManager.run.stage = 6;
    g_GameManager.run.stage_frames = -3;
    g_GameManager.mode.playing = true;
    g_GameManager.mode.paused = 2;
    g_GameManager.mode.in_continue_menu = true;
    g_GameManager.mode.frozen = true;
    g_GameManager.score.value = 100;
    g_GameManager.score.shown = 90;
    g_GameManager.score.next_extend = -1;
    g_GameManager.player.lives = -1;
    g_GameManager.player.bombs = -2;
    g_GameManager.player.power = 123;
    g_GameManager.player.graze = 456;
    g_GameManager.player.point_items = 78;
    g_GameManager.rank.value = 21;
    g_GameManager.rank.sub = -13;
    g_Rng.seed = 0x9876;
    g_Rng.generated = -5;
    g_Player.pos = (VEC3) { -0.0f, 1.5f, 99.0f };
    g_Player.state.value = PLAYER_DYING;
    g_Enemies.spell_card.state = ENEMY_SPELL_TIMED_OUT;
    g_Enemies.spell_card.capturable = true;
    g_Enemies.spell_card.index = -2;
    g_Enemies.spell_card.bonus = -100;
    uint32_t row[TH06_CALC_COUNT];
    RunnerState_Calc(row);
    CHECK_EQ_INT(row[TH06_CALC_buttons], 0x1a5);
    CHECK_EQ_INT(row[TH06_CALC_stage], 6);
    CHECK_EQ_INT(row[TH06_CALC_stage_frames], 0xfffffffdu);
    CHECK_EQ_INT(row[TH06_CALC_flags], 0x01010201);
    CHECK_EQ_INT(row[TH06_CALC_score], 100);
    CHECK_EQ_INT(row[TH06_CALC_gui_score], 90);
    CHECK_EQ_INT(row[TH06_CALC_lives], 0xffffffffu);
    CHECK_EQ_INT(row[TH06_CALC_bombs], 0xfffffffeu);
    CHECK_EQ_INT(row[TH06_CALC_power], 123);
    CHECK_EQ_INT(row[TH06_CALC_graze], 456);
    CHECK_EQ_INT(row[TH06_CALC_point_items], 78);
    CHECK_EQ_INT(row[TH06_CALC_rank], 21);
    CHECK_EQ_INT(row[TH06_CALC_sub_rank], 0xfffffff3u);
    CHECK_EQ_INT(row[TH06_CALC_next_extend], 255);
    CHECK_EQ_INT(row[TH06_CALC_rng_seed], 0x9876);
    CHECK_EQ_INT(row[TH06_CALC_rng_generated], 0xfffffffbu);
    CHECK_EQ_INT(row[TH06_CALC_player_x], 0x80000000u);
    CHECK_EQ_INT(row[TH06_CALC_player_y], 0x3fc00000u);
    CHECK_EQ_INT(row[TH06_CALC_player_state], PLAYER_DYING);
    CHECK_EQ_INT(row[TH06_CALC_spell_card], 0xfffe0102u);
    CHECK_EQ_INT(row[TH06_CALC_spell_card_bonus], 0xffffff9cu);
}

TEST(entity_hashes_include_active_slot_order_and_exact_values)
{
    M_Reset();
    g_Player.shots.bullets[2] = (PLAYER_BULLET) {
        .state = PLAYER_BULLET_HIT,
        .damage = -7,
        .pos = { 1.0f, -0.0f, 2.0f },
    };
    g_Player.shots.bullets[5] = (PLAYER_BULLET) {
        .state = PLAYER_BULLET_FLYING,
        .damage = 32767,
        .pos = { 3.0f, 4.0f, 5.0f },
    };
    g_Player.shots.bullets[1].pos.x = 999.0f;
    g_Bullets.bullets[3] = (BULLET) {
        .state = BULLET_FADING,
        .flags = 0x8123,
        .pos = { 1.0f, 2.0f, 3.0f },
        .velocity = { 4.0f, 5.0f, 6.0f },
        .speed = 7.0f,
        .angle = 8.0f,
        .timer.current = -9,
        .grazed = true,
    };
    g_Bullets.bullets[4].flags = 0xffff;
    g_Bullets.lasers[1] = (LASER) {
        .active = true,
        .state = LASER_ENDING,
        .pos = { 9.0f, 10.0f, 11.0f },
        .angle = 12.0f,
        .start_offset = 13.0f,
        .end_offset = 14.0f,
        .timer.current = 15,
    };
    g_Enemies.enemies[0].flags.active = true;
    g_Enemies.enemies[0].move.pos = (VEC3) { 1.0f, 2.0f, 3.0f };
    g_Enemies.enemies[0].life = -123;
    g_Enemies.enemies[255].flags.active = true;
    g_Enemies.enemies[255].move.pos = (VEC3) { 4.0f, 5.0f, 6.0f };
    g_Enemies.enemies[255].life = 456;
    g_Enemies.enemies[1].life = 999;
    uint32_t row[TH06_CALC_COUNT];
    RunnerState_Calc(row);
    CHECK_EQ_INT(row[TH06_CALC_shot_count], 2);
    CHECK_EQ_INT(row[TH06_CALC_shot_hash], 0xd6e73e36u);
    CHECK_EQ_INT(row[TH06_CALC_bullet_count], 1);
    CHECK_EQ_INT(row[TH06_CALC_bullet_hash], 0x0286c398u);
    CHECK_EQ_INT(row[TH06_CALC_laser_count], 1);
    CHECK_EQ_INT(row[TH06_CALC_laser_hash], 0x66238339u);
    CHECK_EQ_INT(row[TH06_CALC_enemy_count], 2);
    CHECK_EQ_INT(row[TH06_CALC_enemy_hash], 0x2b8517beu);
    CHECK_EQ_INT(row[TH06_CALC_item_count], 0);
    CHECK_EQ_INT(row[TH06_CALC_item_hash], 0x811c9dc5u);
}

TEST(item_view_keeps_private_slots_visible_through_spawn_and_reset)
{
    M_Reset();
    const ITEM *const items = Item_GetSlots();
    Item_Spawn((VEC3) { 1.0f, -2.0f, 3.5f }, ITEM_STAR, ITEM_HOMING);
    Item_Spawn((VEC3) { 4.0f, 5.0f, 6.0f }, ITEM_POWER, ITEM_FALLING);
    CHECK(Item_GetSlots() == items);
    CHECK(items[0].active);
    CHECK(items[1].active);
    CHECK(!items[2].active);
    CHECK_EQ_INT(items[0].type, ITEM_STAR);
    CHECK_EQ_INT(items[0].state, ITEM_HOMING);
    uint32_t row[TH06_CALC_COUNT];
    RunnerState_Calc(row);
    CHECK_EQ_INT(row[TH06_CALC_item_count], 2);
    CHECK_EQ_INT(row[TH06_CALC_item_hash], 0xc5b17c94u);
    Item_Reset();
    CHECK(Item_GetSlots() == items);
    CHECK(!items[0].active);
    CHECK(!items[1].active);
    RunnerState_Calc(row);
    CHECK_EQ_INT(row[TH06_CALC_item_count], 0);
    CHECK_EQ_INT(row[TH06_CALC_item_hash], 0x811c9dc5u);
}
