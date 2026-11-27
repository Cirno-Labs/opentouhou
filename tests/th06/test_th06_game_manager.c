#include "tests/test.h"
#include "th06/game_manager.h"

#include <math.h>

TEST(screen_sprites_keep_their_pixels_and_use_viewport_depth)
{
    GameManager_Create(&g_GameManager);
    g_GameManager.camera.viewport = (VIEWPORT) { 32, 16, 384, 448, 0, 0.5f };
    GameManager_SetUpFixedCamera(0.0f);
    const MAT4 world = Mat4_Identity();
    const ANM_DRAW screen = GameManager_GetScreenDrawContext();
    const VEC3 player = Vec3_Project(
        (VEC3) { 224, 96, 0.49f }, &screen.view.viewport,
        &screen.view.projection, &screen.view.view, &world);
    CHECK(fabsf(player.x - 224.0f) < 0.0001f);
    CHECK(fabsf(player.y - 96.0f) < 0.0001f);
    CHECK(fabsf(player.z - 0.245f) < 0.000001f);
    CHECK(screen.view.viewport.x == 32.0f);
    CHECK(screen.view.viewport.width == 384.0f);
    const ANM_DRAW camera = GameManager_GetDrawContext();
    const VEC3 enemy = Vec3_Project(
        (VEC3) { 192, -96, 0.494f }, &camera.view.viewport,
        &camera.view.projection, &camera.view.view, &world);
    CHECK(player.z < enemy.z);

    g_GameManager.camera.viewport.min_z = 0.5f;
    g_GameManager.camera.viewport.max_z = 1.0f;
    const GFX_VIEW background = GameManager_GetScreenDrawContext().view;
    const VEC3 back = Vec3_Project(
        (VEC3) { 224, 96, 0.49f }, &background.viewport, &background.projection,
        &background.view, &world);
    CHECK(fabsf(back.z - 0.745f) < 0.000001f);
}

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

TEST(extra_opens_with_a_normal_to_lunatic_clear_without_continues)
{
    GameManager_Create(&g_GameManager);
    CLEAR_RECORD *const clear = &g_GameManager.records.clears[3];
    clear->without_continues[0] = 99;
    clear->with_continues[2] = 99;
    CHECK(!GameManager_IsExtraUnlocked(1, 1));
    for (int32_t difficulty = 1; difficulty <= 3; difficulty++) {
        *clear = (CLEAR_RECORD) {};
        clear->without_continues[difficulty] = 99;
        CHECK(GameManager_IsExtraUnlocked(1, 1));
        CHECK(!GameManager_IsExtraUnlocked(1, 0));
    }
    clear->without_continues[3] = 98;
    CHECK(!GameManager_IsExtraUnlocked(1, 1));
}
