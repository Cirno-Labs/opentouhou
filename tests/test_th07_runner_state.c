#include "../replay/th07/runner/state.h"

#include "test.h"

#include "th07/anm.h"
#include "th07/bullet.h"
#include "th07/enemy.h"
#include "th07/game.h"
#include "th07/game_manager.h"
#include "th07/item.h"
#include "th07/player.h"

#include "core/base/memory.h"

#include <string.h>

static_assert(_Generic(Item_GetSlots(), const ITEM *: 1, default: 0));

static void M_Reset(void)
{
    memset(&g_GameManager, 0, sizeof(g_GameManager));
    memset(&g_Player, 0, sizeof(g_Player));
    memset(&g_Bullets, 0, sizeof(g_Bullets));
    memset(&g_Enemies, 0, sizeof(g_Enemies));
    memset(&g_Rng, 0, sizeof(g_Rng));
    memset(&g_Buttons, 0, sizeof(g_Buttons));
    g_GameSpeed = 1.0f;
    static bool anm_ready;
    if (!anm_ready) {
        Anm_Init(&g_HeapAllocator, &g_Rng);
        anm_ready = true;
    }
    Item_Reset();
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
    g_GameManager.cherry.offset = 1000;
    g_GameManager.cherry.value = 1010;
    g_GameManager.cherry.max = 1020;
    g_GameManager.cherry.plus = 1030;
    g_Rng.seed = 0xabcd;
    uint32_t row[TH07_START_COUNT];
    RunnerState_Start(row);
    CHECK_EQ_INT(row[TH07_START_difficulty], 4);
    CHECK_EQ_INT(row[TH07_START_character], 1);
    CHECK_EQ_INT(row[TH07_START_shot_type], 1);
    CHECK_EQ_INT(row[TH07_START_stage], 7);
    CHECK_EQ_INT(row[TH07_START_lives], 0xbf800000u);
    CHECK_EQ_INT(row[TH07_START_bombs], 0xc0000000u);
    CHECK_EQ_INT(row[TH07_START_power], 0x43000000u);
    CHECK_EQ_INT(row[TH07_START_score], 123456780);
    CHECK_EQ_INT(row[TH07_START_rng_seed], 0xabcd);
    CHECK_EQ_INT(row[TH07_START_rank], 19);
    CHECK_EQ_INT(row[TH07_START_sub_rank], 0xfffffff9u);
    CHECK_EQ_INT(row[TH07_START_cherry], 10);
    CHECK_EQ_INT(row[TH07_START_cherry_max], 20);
    CHECK_EQ_INT(row[TH07_START_cherry_plus], 30);
}

TEST(calc_preserves_flags_cherry_offset_and_float_bits)
{
    M_Reset();
    g_Buttons.play_word = 0x1a5;
    g_GameManager.run.stage = 6;
    g_GameManager.run.stage_frames = -3;
    g_GameManager.mode.playing = true;
    g_GameManager.mode.paused = 2;
    g_GameManager.mode.in_continue_menu = true;
    g_GameSpeed = 0.5f;
    g_GameManager.score.value = 100;
    g_GameManager.score.shown = 90;
    g_GameManager.player.lives = -1;
    g_GameManager.player.bombs = -2;
    g_GameManager.player.power = 123;
    g_GameManager.player.graze = 456;
    g_GameManager.player.graze_total = 789;
    g_GameManager.player.point_items = 78;
    g_GameManager.player.point_items_total = 90;
    g_GameManager.cherry.offset = 1000;
    g_GameManager.cherry.value = 1010;
    g_GameManager.cherry.max = 1020;
    g_GameManager.cherry.plus = 1030;
    g_GameManager.rank.value = 21;
    g_GameManager.rank.sub = -13;
    g_GameManager.stats.deaths = 2.0f;
    g_GameManager.stats.bombs_used = 3.0f;
    g_GameManager.stats.spell_cards_captured = 5;
    g_Rng.seed = 0x9876;
    g_Rng.generated = -5;
    g_Player.pos = (VEC3) { -0.0f, 1.5f, 99.0f };
    g_Player.state.value = PLAYER_DYING;
    g_Enemies.spell_card.state = ENEMY_SPELL_TIMED_OUT;
    g_Enemies.spell_card.capturable = true;
    g_Enemies.spell_card.index = -2;
    g_Enemies.spell_card.bonus = -100;
    uint32_t row[TH07_CALC_COUNT];
    RunnerState_Calc(row);
    CHECK_EQ_INT(row[TH07_CALC_buttons], 0x1a5);
    CHECK_EQ_INT(row[TH07_CALC_events], 0);
    CHECK_EQ_INT(row[TH07_CALC_stage], 6);
    CHECK_EQ_INT(row[TH07_CALC_stage_frames], 0xfffffffdu);
    CHECK_EQ_INT(row[TH07_CALC_flags], 0x00010204);
    CHECK_EQ_INT(row[TH07_CALC_game_speed], 0x3f000000u);
    CHECK_EQ_INT(row[TH07_CALC_score], 100);
    CHECK_EQ_INT(row[TH07_CALC_gui_score], 90);
    CHECK_EQ_INT(row[TH07_CALC_lives], 0xbf800000u);
    CHECK_EQ_INT(row[TH07_CALC_bombs], 0xc0000000u);
    CHECK_EQ_INT(row[TH07_CALC_power], 0x42f60000u);
    CHECK_EQ_INT(row[TH07_CALC_graze], 456);
    CHECK_EQ_INT(row[TH07_CALC_graze_total], 789);
    CHECK_EQ_INT(row[TH07_CALC_point_items], 78);
    CHECK_EQ_INT(row[TH07_CALC_point_items_total], 90);
    CHECK_EQ_INT(row[TH07_CALC_cherry], 10);
    CHECK_EQ_INT(row[TH07_CALC_cherry_max], 20);
    CHECK_EQ_INT(row[TH07_CALC_cherry_plus], 30);
    CHECK_EQ_INT(row[TH07_CALC_rank], 21);
    CHECK_EQ_INT(row[TH07_CALC_sub_rank], 0xfffffff3u);
    CHECK_EQ_INT(row[TH07_CALC_deaths], 2);
    CHECK_EQ_INT(row[TH07_CALC_bombs_used], 3);
    CHECK_EQ_INT(row[TH07_CALC_spell_cards_captured], 5);
    CHECK_EQ_INT(row[TH07_CALC_rng_seed], 0x9876);
    CHECK_EQ_INT(row[TH07_CALC_rng_generated], 0xfffffffbu);
    CHECK_EQ_INT(row[TH07_CALC_player_x], 0x80000000u);
    CHECK_EQ_INT(row[TH07_CALC_player_y], 0x3fc00000u);
    CHECK_EQ_INT(row[TH07_CALC_player_state], PLAYER_DYING);
    CHECK_EQ_INT(row[TH07_CALC_spell_card], 0xfffe0102u);
    CHECK_EQ_INT(row[TH07_CALC_spell_card_bonus], 0xffffff9cu);
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
    uint32_t row[TH07_CALC_COUNT];
    RunnerState_Calc(row);
    CHECK_EQ_INT(row[TH07_CALC_item_count], 2);
    Item_Reset();
    CHECK(Item_GetSlots() == items);
    RunnerState_Calc(row);
    CHECK_EQ_INT(row[TH07_CALC_item_count], 0);
    CHECK_EQ_INT(row[TH07_CALC_item_hash], 0x811c9dc5u);
}

TEST(hashes_fold_a_timer_fraction)
{
    M_Reset();
    g_Enemies.enemies[0].flags.active = true;
    uint32_t row[TH07_CALC_COUNT];
    RunnerState_Calc(row);
    const uint32_t whole = row[TH07_CALC_enemy_hash];
    g_Enemies.enemies[0].script.state.timer.sub_frame = 0.5f;
    RunnerState_Calc(row);
    CHECK(row[TH07_CALC_enemy_hash] != whole);
}
