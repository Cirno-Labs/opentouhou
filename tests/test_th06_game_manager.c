#include "th06/game_manager.h"

#include "test.h"

#include <math.h>

TEST(create_zeroes_and_places_the_play_area)
{
    g_GameManager.player.lives = 3;
    GameManager_Create(&g_GameManager);
    CHECK_EQ_INT(g_GameManager.player.lives, 0);
    CHECK(g_GameManager.play_area.x == 32.0f);
    CHECK(g_GameManager.play_area.y == 16.0f);
    CHECK(g_GameManager.play_area.width == 384.0f);
    CHECK(g_GameManager.play_area.height == 448.0f);
}

TEST(a_box_touching_an_edge_is_in_the_play_area)
{
    GameManager_Create(&g_GameManager);
    CHECK(GameManager_IsInPlayArea(-4.0f, 100.0f, 8.0f, 8.0f));
    CHECK(GameManager_IsInPlayArea(388.0f, 100.0f, 8.0f, 8.0f));
    CHECK(!GameManager_IsInPlayArea(-4.5f, 100.0f, 8.0f, 8.0f));
    CHECK(!GameManager_IsInPlayArea(100.0f, 452.5f, 8.0f, 8.0f));
}

TEST(the_play_area_check_reads_its_current_size)
{
    GameManager_Create(&g_GameManager);
    g_GameManager.play_area.width = 376.0f;
    CHECK(!GameManager_IsInPlayArea(381.0f, 100.0f, 8.0f, 8.0f));
}

TEST(a_nan_position_is_in_the_play_area)
{
    GameManager_Create(&g_GameManager);
    CHECK(GameManager_IsInPlayArea(NAN, NAN, 8.0f, 8.0f));
}

TEST(add_rank_carries_hundredths_and_stops_at_max)
{
    GameManager_Create(&g_GameManager);
    g_GameManager.rank.value = 10;
    g_GameManager.rank.max = 12;
    GameManager_AddRank(150);
    CHECK_EQ_INT(g_GameManager.rank.value, 11);
    CHECK_EQ_INT(g_GameManager.rank.sub, 50);
    GameManager_AddRank(300);
    CHECK_EQ_INT(g_GameManager.rank.value, 12);
    CHECK_EQ_INT(g_GameManager.rank.sub, 50);
}

TEST(sub_rank_borrows_hundredths_and_stops_at_min)
{
    GameManager_Create(&g_GameManager);
    g_GameManager.rank.value = 10;
    g_GameManager.rank.min = 9;
    GameManager_SubRank(50);
    CHECK_EQ_INT(g_GameManager.rank.value, 9);
    CHECK_EQ_INT(g_GameManager.rank.sub, 50);
    GameManager_SubRank(250);
    CHECK_EQ_INT(g_GameManager.rank.value, 9);
    CHECK_EQ_INT(g_GameManager.rank.sub, 0);
}
