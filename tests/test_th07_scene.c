#include "th07/scene.h"

#include "test.h"
#include "th07_anm_builder.h"
#include "th07_data.h"
#include "th07_scene_stubs.h"
#include "shell_fake.h"
#include "test_dir.h"

#include "th07/buttons.h"
#include "th07/files.h"
#include "th07/game.h"
#include "th07/settings.h"
#include "th07/stubs.h"

#include "core/base/memory.h"
#include "core/io/file.h"

#include <string.h>

static const TH07_INPUT m_NoInput;
static const TH07_INPUT m_Shoot = { .shoot = true };

static void M_SetDevice(const INPUT_DEVICE device, const TH07_INPUT *input)
{
    FakeShell_SetDeviceInput(device, input, sizeof(*input));
}

// Mounts a data directory with the sprite files that the game loads at
// start-up.
static void M_MountFiles(void)
{
    static bool mounted;
    if (!mounted) {
        static const TH07_DATA_FILE version = { "th07_0100b.ver", "0100b" };
        static char *directory;
        directory = TestDir_Prepare("scene");
        Th07Data_WriteArchive(directory, &version, 1);
        Th07Data_WriteMusic(directory, nullptr, 0);
        char *const data = TestDir_Join(directory, "data");
        CHECK(Result_Log(File_MakeDir(data), LOG_LEVEL_ERROR));
        Mem_Free(data);
        TH07_ANM_BUILDER builder;
        Th07AnmBuilder_Init(&builder);
        TH07_ANM_BUILDER_ANM *const anm =
            Th07AnmBuilder_Add(&builder, 32, 32, "@");
        Th07AnmBuilder_AddSprite(anm, 0, 0.0f, 0.0f, 16.0f, 16.0f);
        Th07AnmBuilder_WriteFile(&builder, directory, "data/text.anm");
        Th07AnmBuilder_WriteFile(&builder, directory, "data/ascii.anm");
        Th07AnmBuilder_WriteFile(&builder, directory, "data/capture.anm");
        CHECK(Result_Log(Th07Data_Mount(directory), LOG_LEVEL_ERROR));
        mounted = true;
    }
}

static void M_Setup(void)
{
    M_MountFiles();
    SceneStubCalls_Reset();
    Chain_Release(&g_Chain);
    memset(&g_Buttons, 0, sizeof(g_Buttons));
    FakeShell_Reset();
    for (int32_t device = 0; device < INPUT_DEVICE_COUNT; device++) {
        M_SetDevice((INPUT_DEVICE)device, &m_NoInput);
    }
    g_Settings.shoot_focuses = true;
    Result_Ignore(Game_Init());
}

static void M_StartGameplay(const int32_t stage)
{
    M_Setup();
    Game_Tick(&m_NoInput);
    Scene_Request(SCENE_GAMEPLAY);
    Game_Tick(&m_NoInput);
    g_GameManager.run.stage = stage;
}

TEST(boot_goes_to_title)
{
    M_Setup();
    CHECK_EQ_INT(Scene_GetRequested(), SCENE_QUIT);
    CHECK(Game_Tick(&m_NoInput));
    CHECK_EQ_INT(Scene_Get(), SCENE_TITLE);
    CHECK_EQ_INT(g_SceneStubCalls.title_set_up, 1);
    CHECK_EQ_INT(g_SceneStubCalls.title_set_up_mode, 0);
}

TEST(title_stays_without_a_request)
{
    M_Setup();
    for (int tick = 0; tick < 10; tick++) {
        CHECK(Game_Tick(&m_NoInput));
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
    Game_Tick(&m_NoInput);
    Scene_Request(SCENE_QUIT);
    CHECK(!Game_Tick(&m_NoInput));
}

TEST(reset_starts_again_from_boot)
{
    M_Setup();
    Game_Tick(&m_NoInput);
    Scene_Request(SCENE_RESET);
    CHECK(Game_Tick(&m_NoInput));
    CHECK_EQ_INT(Scene_Get(), SCENE_BOOT);
    CHECK(Game_Tick(&m_NoInput));
    CHECK_EQ_INT(Scene_Get(), SCENE_TITLE);
    CHECK_EQ_INT(g_SceneStubCalls.title_set_up, 2);
}

TEST(title_goes_to_scores_and_music_room_and_back)
{
    M_Setup();
    Game_Tick(&m_NoInput);
    Scene_Request(SCENE_SCORES);
    Game_Tick(&m_NoInput);
    CHECK_EQ_INT(Scene_Get(), SCENE_SCORES);
    CHECK_EQ_INT(g_SceneStubCalls.result_screen_register_mode, 0);
    Scene_Request(SCENE_TITLE);
    Game_Tick(&m_NoInput);
    Scene_Request(SCENE_MUSIC_ROOM);
    Game_Tick(&m_NoInput);
    CHECK_EQ_INT(Scene_Get(), SCENE_MUSIC_ROOM);
    CHECK_EQ_INT(g_SceneStubCalls.music_screen_register, 1);
    Scene_Request(SCENE_TITLE);
    Game_Tick(&m_NoInput);
    CHECK_EQ_INT(Scene_Get(), SCENE_TITLE);
    CHECK_EQ_INT(g_SceneStubCalls.title_set_up, 3);
}

TEST(the_title_sees_the_scene_it_came_from)
{
    M_Setup();
    Game_Tick(&m_NoInput);
    CHECK_EQ_INT(g_SceneStubCalls.title_set_up_previous, SCENE_BOOT);
    Scene_Request(SCENE_SCORES);
    Game_Tick(&m_NoInput);
    Scene_Request(SCENE_TITLE);
    Game_Tick(&m_NoInput);
    CHECK_EQ_INT(g_SceneStubCalls.title_set_up_previous, SCENE_SCORES);
    CHECK_EQ_INT(Scene_GetPrevious(), SCENE_SCORES);
}

TEST(next_stage_reregisters_the_game)
{
    M_StartGameplay(2);
    Scene_Request(SCENE_NEXT_STAGE);
    CHECK(Game_Tick(&m_NoInput));
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
    Game_Tick(&m_NoInput);
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
    Game_Tick(&m_NoInput);
    CHECK_EQ_INT(g_GameManager.run.stage, 3);

    M_StartGameplay(7);
    g_GameManager.run.difficulty = 4;
    Scene_Request(SCENE_RETRY);
    Game_Tick(&m_NoInput);
    CHECK_EQ_INT(g_GameManager.run.stage, 6);
}

TEST(restart_stage_replays_the_stage_as_a_next_stage)
{
    M_StartGameplay(3);
    Scene_Request(SCENE_RESTART_STAGE);
    Game_Tick(&m_NoInput);
    CHECK_EQ_INT(Scene_Get(), SCENE_GAMEPLAY);
    CHECK_EQ_INT(g_GameManager.run.stage, 2);
    CHECK_EQ_INT(
        g_SceneStubCalls.game_manager_register_scene, SCENE_NEXT_STAGE);
}

TEST(next_stage_again_registers_as_a_next_stage)
{
    M_StartGameplay(3);
    Scene_Request(SCENE_NEXT_STAGE_AGAIN);
    Game_Tick(&m_NoInput);
    CHECK_EQ_INT(Scene_Get(), SCENE_GAMEPLAY);
    CHECK_EQ_INT(g_GameManager.run.stage, 3);
    CHECK_EQ_INT(
        g_SceneStubCalls.game_manager_register_scene, SCENE_NEXT_STAGE);
}

TEST(gameplay_to_title_discards_the_replay_through_boot)
{
    M_StartGameplay(1);
    Scene_Request(SCENE_TITLE);
    Game_Tick(&m_NoInput);
    CHECK_EQ_INT(Scene_Get(), SCENE_TITLE);
    CHECK_EQ_INT(g_SceneStubCalls.replay_discard, 1);
    CHECK_EQ_INT(g_SceneStubCalls.replay_discard_scene, SCENE_BOOT);
    CHECK_EQ_INT(g_SceneStubCalls.title_set_up_mode, 0);
}

TEST(replay_end_goes_to_the_title_in_mode_1)
{
    M_StartGameplay(1);
    Scene_Request(SCENE_REPLAY_END);
    Game_Tick(&m_NoInput);
    CHECK_EQ_INT(Scene_Get(), SCENE_TITLE);
    CHECK_EQ_INT(g_SceneStubCalls.replay_discard, 1);
    CHECK_EQ_INT(g_SceneStubCalls.title_set_up_mode, 1);
}

TEST(ending_counts_a_clear_of_the_difficulty)
{
    M_StartGameplay(6);
    g_GameManager.run.difficulty = 2;
    Scene_Request(SCENE_ENDING);
    Game_Tick(&m_NoInput);
    CHECK_EQ_INT(Scene_Get(), SCENE_ENDING);
    CHECK_EQ_INT(g_GameManager.play_counts[2].clears, 1);
    CHECK_EQ_INT(g_SceneStubCalls.ending_register, 1);
    Scene_Request(SCENE_GAME_RESULT);
    Game_Tick(&m_NoInput);
    CHECK_EQ_INT(Scene_Get(), SCENE_GAME_RESULT);
    CHECK_EQ_INT(g_SceneStubCalls.result_screen_register_mode, 1);
}

TEST(game_result_discards_the_replay_on_quit)
{
    M_StartGameplay(1);
    Scene_Request(SCENE_GAME_RESULT);
    Game_Tick(&m_NoInput);
    Scene_Request(SCENE_QUIT);
    CHECK(!Game_Tick(&m_NoInput));
    CHECK_EQ_INT(g_SceneStubCalls.replay_discard, 1);
}

TEST(a_switch_clears_the_buttons)
{
    M_Setup();
    Game_Tick(&m_NoInput);

    const TH07_INPUT input = { .shoot = true };
    Game_Tick(&input);
    CHECK(g_Buttons.held.shoot);

    Scene_Request(SCENE_GAMEPLAY);
    Game_Tick(&input);
    CHECK(!g_Buttons.held.shoot);
    CHECK_EQ_INT(g_Buttons.word, 0);
}

TEST(a_dropped_calc_merges_the_next_buttons)
{
    M_Setup();
    Game_Tick(&m_NoInput);
    const TH07_INPUT shoot = { .shoot = true };
    Game_Tick(&shoot);
    g_GameManager.mode.calc_dropped = true;
    const TH07_INPUT bomb = { .bomb = true };
    Game_Tick(&bomb);
    CHECK_EQ_INT(g_Buttons.word, 0x3);
    CHECK_EQ_INT(g_Buttons.previous_word, 0);
}

TEST(the_shoot_focuses_setting_reaches_the_buttons)
{
    M_Setup();
    M_SetDevice(INPUT_DEVICE_GAMEPAD, &m_Shoot);
    for (int32_t calc = 0; calc < 10; calc++) {
        Game_Tick(&m_Shoot);
    }
    CHECK(g_Buttons.held.focus);

    M_Setup();
    g_Settings.shoot_focuses = false;
    M_SetDevice(INPUT_DEVICE_GAMEPAD, &m_Shoot);
    for (int32_t calc = 0; calc < 10; calc++) {
        Game_Tick(&m_Shoot);
    }
    CHECK(!g_Buttons.held.focus);
    CHECK_EQ_INT(g_Buttons.shot_held_frames, 0);
}
