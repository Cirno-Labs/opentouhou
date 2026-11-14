#include "th07/scene.h"

#include "test.h"
#include "th07_scene_stubs.h"

#include "th07/game.h"
#include "th07/stubs.h"

static void M_Setup(void)
{
    SceneStubCalls_Reset();
    Chain_Release(&g_Chain);
    Result_Ignore(Game_Init());
}

static void M_StartGameplay(const int32_t stage)
{
    M_Setup();
    Game_Tick();
    Scene_Request(SCENE_GAMEPLAY);
    Game_Tick();
    g_GameManager.run.stage = stage;
}

TEST(boot_goes_to_title)
{
    M_Setup();
    CHECK_EQ_INT(Scene_GetRequested(), SCENE_QUIT);
    CHECK(Game_Tick());
    CHECK_EQ_INT(Scene_Get(), SCENE_TITLE);
    CHECK_EQ_INT(g_SceneStubCalls.title_set_up, 1);
    CHECK_EQ_INT(g_SceneStubCalls.title_set_up_mode, 0);
}

TEST(title_stays_without_a_request)
{
    M_Setup();
    for (int tick = 0; tick < 10; tick++) {
        CHECK(Game_Tick());
    }
    CHECK_EQ_INT(Scene_Get(), SCENE_TITLE);
    CHECK_EQ_INT(g_SceneStubCalls.title_set_up, 1);
}

TEST(title_goes_to_gameplay)
{
    M_StartGameplay(1);
    CHECK_EQ_INT(Scene_Get(), SCENE_GAMEPLAY);
    CHECK_EQ_INT(g_SceneStubCalls.game_manager_register, 1);
}

TEST(quit_from_title_ends_the_tick_with_false)
{
    M_Setup();
    Game_Tick();
    Scene_Request(SCENE_QUIT);
    CHECK(!Game_Tick());
}

TEST(reset_starts_again_from_boot)
{
    M_Setup();
    Game_Tick();
    Scene_Request(SCENE_RESET);
    CHECK(Game_Tick());
    CHECK_EQ_INT(Scene_Get(), SCENE_BOOT);
    CHECK(Game_Tick());
    CHECK_EQ_INT(Scene_Get(), SCENE_TITLE);
    CHECK_EQ_INT(g_SceneStubCalls.title_set_up, 2);
}

TEST(title_goes_to_scores_and_music_room_and_back)
{
    M_Setup();
    Game_Tick();
    Scene_Request(SCENE_SCORES);
    Game_Tick();
    CHECK_EQ_INT(Scene_Get(), SCENE_SCORES);
    CHECK_EQ_INT(g_SceneStubCalls.result_screen_register_mode, 0);
    Scene_Request(SCENE_TITLE);
    Game_Tick();
    Scene_Request(SCENE_MUSIC_ROOM);
    Game_Tick();
    CHECK_EQ_INT(Scene_Get(), SCENE_MUSIC_ROOM);
    CHECK_EQ_INT(g_SceneStubCalls.music_screen_register, 1);
    Scene_Request(SCENE_TITLE);
    Game_Tick();
    CHECK_EQ_INT(Scene_Get(), SCENE_TITLE);
    CHECK_EQ_INT(g_SceneStubCalls.title_set_up, 3);
}

TEST(next_stage_reregisters_the_game)
{
    M_StartGameplay(2);
    Scene_Request(SCENE_NEXT_STAGE);
    CHECK(Game_Tick());
    CHECK_EQ_INT(Scene_Get(), SCENE_GAMEPLAY);
    CHECK_EQ_INT(g_SceneStubCalls.game_manager_remove, 1);
    CHECK_EQ_INT(g_SceneStubCalls.game_manager_register, 2);
    CHECK_EQ_INT(g_GameManager.run.stage, 2);
}

TEST(retry_starts_a_game_from_the_first_stage)
{
    M_StartGameplay(4);
    g_GameManager.run.difficulty = 3;
    Scene_Request(SCENE_RETRY);
    Game_Tick();
    CHECK_EQ_INT(Scene_Get(), SCENE_GAMEPLAY);
    CHECK_EQ_INT(g_GameManager.run.stage, 0);
    CHECK_EQ_INT(g_SceneStubCalls.replay_discard, 1);
    CHECK_EQ_INT(g_SceneStubCalls.replay_discard_scene, SCENE_RETRY);
    CHECK_EQ_INT(g_SceneStubCalls.game_manager_register, 2);
}

TEST(retry_replays_the_stage_in_a_practice_and_in_extra)
{
    M_StartGameplay(4);
    g_GameManager.mode.in_practice = true;
    Scene_Request(SCENE_RETRY);
    Game_Tick();
    CHECK_EQ_INT(g_GameManager.run.stage, 3);

    M_StartGameplay(7);
    g_GameManager.run.difficulty = 4;
    Scene_Request(SCENE_RETRY);
    Game_Tick();
    CHECK_EQ_INT(g_GameManager.run.stage, 6);
}

TEST(restart_stage_replays_the_stage_as_a_next_stage)
{
    M_StartGameplay(3);
    Scene_Request(SCENE_RESTART_STAGE);
    Game_Tick();
    CHECK_EQ_INT(Scene_Get(), SCENE_GAMEPLAY);
    CHECK_EQ_INT(g_GameManager.run.stage, 2);
    CHECK_EQ_INT(
        g_SceneStubCalls.game_manager_register_scene, SCENE_NEXT_STAGE);
}

TEST(next_stage_again_registers_as_a_next_stage)
{
    M_StartGameplay(3);
    Scene_Request(SCENE_NEXT_STAGE_AGAIN);
    Game_Tick();
    CHECK_EQ_INT(Scene_Get(), SCENE_GAMEPLAY);
    CHECK_EQ_INT(g_GameManager.run.stage, 3);
    CHECK_EQ_INT(
        g_SceneStubCalls.game_manager_register_scene, SCENE_NEXT_STAGE);
}

TEST(gameplay_to_title_discards_the_replay_through_boot)
{
    M_StartGameplay(1);
    Scene_Request(SCENE_TITLE);
    Game_Tick();
    CHECK_EQ_INT(Scene_Get(), SCENE_TITLE);
    CHECK_EQ_INT(g_SceneStubCalls.replay_discard, 1);
    CHECK_EQ_INT(g_SceneStubCalls.replay_discard_scene, SCENE_BOOT);
    CHECK_EQ_INT(g_SceneStubCalls.title_set_up_mode, 0);
}

TEST(replay_end_goes_to_the_title_in_mode_1)
{
    M_StartGameplay(1);
    Scene_Request(SCENE_REPLAY_END);
    Game_Tick();
    CHECK_EQ_INT(Scene_Get(), SCENE_TITLE);
    CHECK_EQ_INT(g_SceneStubCalls.replay_discard, 1);
    CHECK_EQ_INT(g_SceneStubCalls.title_set_up_mode, 1);
}

TEST(ending_counts_a_clear_of_the_difficulty)
{
    M_StartGameplay(6);
    g_GameManager.run.difficulty = 2;
    Scene_Request(SCENE_ENDING);
    Game_Tick();
    CHECK_EQ_INT(Scene_Get(), SCENE_ENDING);
    CHECK_EQ_INT(g_GameManager.play_counts[2].clears, 1);
    CHECK_EQ_INT(g_SceneStubCalls.ending_register, 1);
    Scene_Request(SCENE_GAME_RESULT);
    Game_Tick();
    CHECK_EQ_INT(Scene_Get(), SCENE_GAME_RESULT);
    CHECK_EQ_INT(g_SceneStubCalls.result_screen_register_mode, 1);
}

TEST(game_result_discards_the_replay_on_quit)
{
    M_StartGameplay(1);
    Scene_Request(SCENE_GAME_RESULT);
    Game_Tick();
    Scene_Request(SCENE_QUIT);
    CHECK(!Game_Tick());
    CHECK_EQ_INT(g_SceneStubCalls.replay_discard, 1);
}
