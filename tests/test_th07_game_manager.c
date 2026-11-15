#include "th07/game_manager.h"

#include "test.h"

#include "th07/player.h"
#include "th07/stubs.h"

#include "stg/rng.h"

#include <math.h>

#define M_CHERRY_OFFSET 54321

RNG g_Rng;
PLAYER g_Player;

static int32_t m_BorderStarts;
static int32_t m_FullPopups;
static int32_t m_FullPopupCherry;

void Player_StartBorder(void)
{
    m_BorderStarts++;
}

void Gui_ShowCherryFullPopup(const int32_t cherry)
{
    m_FullPopups++;
    m_FullPopupCherry = cherry;
}

static void M_Setup(void)
{
    GameManager_Create(&g_GameManager);
    g_Player = (PLAYER) {};
    m_BorderStarts = 0;
    m_FullPopups = 0;
    m_FullPopupCherry = 0;
}

static void M_SetCaptures(const int32_t count)
{
    for (int32_t i = 0; i < count; i++) {
        g_GameManager.records.spell_captures[i] = 1;
    }
}

TEST(create_zeroes_and_sets_the_starting_values)
{
    g_GameManager.player.lives = 3.0f;
    g_GameManager.mode.demo_index = 0;
    GameManager_Create(&g_GameManager);
    CHECK(g_GameManager.player.lives == 0.0f);
    CHECK(g_GameManager.play_area.x == 32.0f);
    CHECK(g_GameManager.play_area.y == 16.0f);
    CHECK(g_GameManager.play_area.width == 384.0f);
    CHECK(g_GameManager.play_area.height == 448.0f);
    CHECK_EQ_INT(g_GameManager.mode.demo_index, 2);
    CHECK_EQ_INT(g_GameManager.mode.replay_fps, 60);
    CHECK(g_GameManager.records.phantasm_was_open);
}

TEST(a_box_touching_an_edge_is_in_the_play_area)
{
    M_Setup();
    CHECK(GameManager_IsInPlayArea(-4.0f, 100.0f, 8.0f, 8.0f));
    CHECK(GameManager_IsInPlayArea(388.0f, 100.0f, 8.0f, 8.0f));
    CHECK(GameManager_IsInPlayArea(100.0f, 452.0f, 8.0f, 8.0f));
    CHECK(!GameManager_IsInPlayArea(-4.5f, 100.0f, 8.0f, 8.0f));
    CHECK(!GameManager_IsInPlayArea(388.5f, 100.0f, 8.0f, 8.0f));
    CHECK(!GameManager_IsInPlayArea(100.0f, -4.5f, 8.0f, 8.0f));
    CHECK(!GameManager_IsInPlayArea(100.0f, 452.5f, 8.0f, 8.0f));
}

TEST(the_play_area_check_ignores_the_play_area_size)
{
    M_Setup();
    g_GameManager.play_area.width = 100.0f;
    g_GameManager.play_area.height = 100.0f;
    CHECK(GameManager_IsInPlayArea(300.0f, 400.0f, 8.0f, 8.0f));
}

TEST(a_nan_coordinate_is_in_the_play_area)
{
    M_Setup();
    CHECK(GameManager_IsInPlayArea(NAN, NAN, 8.0f, 8.0f));
}

TEST(extra_opens_with_a_main_clear_without_continues)
{
    for (int32_t difficulty = 0; difficulty < 4; difficulty++) {
        M_Setup();
        g_GameManager.records.clears[3].no_continues[difficulty] = 99;
        CHECK(GameManager_HasExtraOpen(3));
        CHECK(!GameManager_HasExtraOpen(2));
        CHECK(GameManager_AnyShotHasExtraOpen());
    }
    M_Setup();
    g_GameManager.records.clears[3].no_continues[4] = 99;
    g_GameManager.records.clears[3].with_continues[2] = 99;
    g_GameManager.records.clears[3].no_continues[1] = 6;
    CHECK(!GameManager_HasExtraOpen(3));
    CHECK(!GameManager_AnyShotHasExtraOpen());
}

TEST(any_shot_checks_all_six_shots)
{
    M_Setup();
    g_GameManager.records.clears[5].no_continues[0] = 99;
    CHECK(GameManager_AnyShotHasExtraOpen());
}

TEST(phantasm_opens_for_an_extra_clear_with_60_captures)
{
    M_Setup();
    g_GameManager.records.clears[1].no_continues[4] = 99;
    M_SetCaptures(59);
    CHECK(!GameManager_HasPhantasmOpen(1));
    CHECK_EQ_INT(g_GameManager.records.clears[1].no_continues[5], 0);
    M_SetCaptures(60);
    CHECK(!GameManager_HasPhantasmOpen(0));
    CHECK(GameManager_HasPhantasmOpen(1));
    CHECK_EQ_INT(g_GameManager.records.clears[1].no_continues[5], 99);
    for (int32_t i = 0; i < 60; i++) {
        g_GameManager.records.spell_captures[i] = 0;
    }
    CHECK(GameManager_HasPhantasmOpen(1));
}

TEST(phantasm_open_marks_every_shot_that_cleared_extra)
{
    M_Setup();
    g_GameManager.records.clears[2].no_continues[4] = 99;
    g_GameManager.records.clears[4].no_continues[4] = 99;
    CHECK(!GameManager_IsPhantasmOpen());
    M_SetCaptures(60);
    CHECK(GameManager_IsPhantasmOpen());
    CHECK_EQ_INT(g_GameManager.records.clears[2].no_continues[5], 99);
    CHECK_EQ_INT(g_GameManager.records.clears[4].no_continues[5], 99);
    CHECK_EQ_INT(g_GameManager.records.clears[3].no_continues[5], 0);
    M_Setup();
    g_GameManager.records.clears[5].no_continues[5] = 99;
    CHECK(GameManager_IsPhantasmOpen());
}

TEST(score_counts_tens_and_drops_the_remainder)
{
    M_Setup();
    GameManager_AddScore(1239);
    CHECK_EQ_INT(g_GameManager.score.value, 123);
    GameManager_AddScore(-15);
    CHECK_EQ_INT(g_GameManager.score.value, 122);
}

TEST(each_add_takes_two_checksum_numbers)
{
    void (*const adds[])(int32_t) = {
        GameManager_AddLives,  GameManager_AddBombs,     GameManager_AddPower,
        GameManager_AddDeaths, GameManager_AddBombsUsed,
    };
    for (size_t i = 0; i < sizeof(adds) / sizeof(adds[0]); i++) {
        M_Setup();
        Rng_SetSeed(&g_Rng, 0x1234);
        RNG expected = g_Rng;
        Rng_U32InRange(&expected, 100000);
        Rng_U32InRange(&expected, 100000);
        adds[i](3);
        CHECK_EQ_INT(g_Rng.seed, expected.seed);
        CHECK_EQ_INT(g_Rng.generated, 4);
    }
    M_Setup();
    GameManager_AddLives(2);
    GameManager_AddBombs(-1);
    GameManager_AddPower(16);
    GameManager_AddDeaths(1);
    GameManager_AddBombsUsed(1);
    CHECK(g_GameManager.player.lives == 2.0f);
    CHECK(g_GameManager.player.bombs == -1.0f);
    CHECK(g_GameManager.player.power == 16.0f);
    CHECK(g_GameManager.stats.deaths == 1.0f);
    CHECK(g_GameManager.stats.bombs_used == 1.0f);
}

TEST(rank_moves_in_hundredths_within_its_bounds)
{
    M_Setup();
    g_GameManager.rank.value = 16;
    g_GameManager.rank.min = 10;
    g_GameManager.rank.max = 18;
    GameManager_AddRank(250);
    CHECK_EQ_INT(g_GameManager.rank.value, 18);
    CHECK_EQ_INT(g_GameManager.rank.sub, 50);
    GameManager_AddRank(500);
    CHECK_EQ_INT(g_GameManager.rank.value, 18);
    GameManager_SubRank(160);
    CHECK_EQ_INT(g_GameManager.rank.value, 16);
    CHECK_EQ_INT(g_GameManager.rank.sub, 90);
    GameManager_SubRank(2000);
    CHECK_EQ_INT(g_GameManager.rank.value, 10);
}

// Starts the cherry counters at the offset that a new game draws.
static void M_SetUpCherry(const int32_t max)
{
    g_GameManager.cherry.offset = M_CHERRY_OFFSET;
    g_GameManager.cherry.value = M_CHERRY_OFFSET;
    g_GameManager.cherry.max = M_CHERRY_OFFSET + max;
    g_GameManager.cherry.plus = M_CHERRY_OFFSET;
}

TEST(cherry_adds_up_to_its_maximum_and_fills_the_plus)
{
    M_Setup();
    M_SetUpCherry(1000);
    GameManager_AddCherry(400);
    CHECK_EQ_INT(g_GameManager.cherry.value, M_CHERRY_OFFSET + 400);
    CHECK_EQ_INT(g_GameManager.cherry.plus, M_CHERRY_OFFSET + 400);
    CHECK_EQ_INT(m_FullPopups, 0);
    GameManager_AddCherry(700);
    CHECK_EQ_INT(g_GameManager.cherry.value, M_CHERRY_OFFSET + 1000);
    CHECK_EQ_INT(g_GameManager.cherry.plus, M_CHERRY_OFFSET + 1100);
    CHECK_EQ_INT(m_FullPopups, 1);
    CHECK_EQ_INT(m_FullPopupCherry, 1000);
    GameManager_AddCherry(10);
    CHECK_EQ_INT(m_FullPopups, 1);
    CHECK_EQ_INT(g_GameManager.cherry.plus, M_CHERRY_OFFSET + 1110);
}

TEST(a_full_plus_starts_the_border_once)
{
    M_Setup();
    M_SetUpCherry(100000);
    GameManager_AddCherry(49999);
    CHECK_EQ_INT(m_BorderStarts, 0);
    GameManager_AddCherry(5);
    CHECK_EQ_INT(g_GameManager.cherry.plus, M_CHERRY_OFFSET + 50000);
    CHECK_EQ_INT(m_BorderStarts, 1);
    g_Player.border_state = PLAYER_BORDER_RUNNING;
    GameManager_AddCherry(5);
    CHECK_EQ_INT(g_GameManager.cherry.plus, M_CHERRY_OFFSET + 50000);
    CHECK_EQ_INT(m_BorderStarts, 1);
    CHECK_EQ_INT(g_GameManager.cherry.value, M_CHERRY_OFFSET + 50009);
}

TEST(a_cherry_loss_leaves_the_plus)
{
    M_Setup();
    M_SetUpCherry(1000);
    g_GameManager.cherry.value += 500;
    g_GameManager.cherry.plus += 300;
    GameManager_AddCherry(-100);
    CHECK_EQ_INT(g_GameManager.cherry.value, M_CHERRY_OFFSET + 400);
    CHECK_EQ_INT(g_GameManager.cherry.plus, M_CHERRY_OFFSET + 300);
}

TEST(cherry_without_plus_still_shows_the_full_popup)
{
    M_Setup();
    M_SetUpCherry(1000);
    GameManager_AddCherryNoPlus(1000);
    CHECK_EQ_INT(g_GameManager.cherry.value, M_CHERRY_OFFSET + 1000);
    CHECK_EQ_INT(g_GameManager.cherry.plus, M_CHERRY_OFFSET);
    CHECK_EQ_INT(m_FullPopups, 1);
    CHECK_EQ_INT(m_FullPopupCherry, 1000);
    M_Setup();
    M_SetUpCherry(1000);
    GameManager_AddCherryQuiet(2000);
    CHECK_EQ_INT(g_GameManager.cherry.value, M_CHERRY_OFFSET + 1000);
    CHECK_EQ_INT(g_GameManager.cherry.plus, M_CHERRY_OFFSET);
    CHECK_EQ_INT(m_FullPopups, 0);
}

TEST(cherry_max_rises_up_to_its_limit)
{
    M_Setup();
    M_SetUpCherry(9999000);
    GameManager_RaiseCherryMax(980);
    CHECK_EQ_INT(g_GameManager.cherry.max, M_CHERRY_OFFSET + 9999980);
    GameManager_RaiseCherryMax(10);
    CHECK_EQ_INT(g_GameManager.cherry.max, M_CHERRY_OFFSET + 9999990);
    GameManager_RaiseCherryMax(10000);
    CHECK_EQ_INT(g_GameManager.cherry.max, M_CHERRY_OFFSET + 9999990);
}

TEST(cherry_falls_down_to_its_offset)
{
    M_Setup();
    M_SetUpCherry(1000);
    g_GameManager.cherry.value += 500;
    GameManager_SubCherry(500);
    CHECK_EQ_INT(g_GameManager.cherry.value, M_CHERRY_OFFSET);
    g_GameManager.cherry.value += 500;
    GameManager_SubCherry(501);
    CHECK_EQ_INT(g_GameManager.cherry.value, M_CHERRY_OFFSET);
    g_GameManager.cherry.value += 500;
    GameManager_SubCherry(100);
    CHECK_EQ_INT(g_GameManager.cherry.value, M_CHERRY_OFFSET + 400);
}
