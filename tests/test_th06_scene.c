#include "th06/scene.h"

#include "test.h"
#include "th06_scene_stubs.h"

#include "th06/buttons.h"
#include "th06/game.h"

static const TH06_INPUT m_NoInput;

static void M_Setup(void)
{
    SceneStubCalls_Reset();
    Chain_Release(&g_Chain);
    Result_Ignore(Game_Init());
}

TEST(boot_goes_to_title)
{
    M_Setup();
    CHECK(Game_Tick(&m_NoInput));
    CHECK_EQ_INT(Scene_Get(), SCENE_TITLE);
    CHECK_EQ_INT(g_SceneStubCalls.title_set_up, 1);
}

TEST(title_goes_to_gameplay)
{
    M_Setup();
    Game_Tick(&m_NoInput);
    Scene_Request(SCENE_GAMEPLAY);
    CHECK(Game_Tick(&m_NoInput));
    CHECK_EQ_INT(Scene_Get(), SCENE_GAMEPLAY);
    CHECK_EQ_INT(g_SceneStubCalls.game_manager_register, 1);
}

TEST(gameplay_next_stage_reregisters)
{
    M_Setup();
    Game_Tick(&m_NoInput);
    Scene_Request(SCENE_GAMEPLAY);
    Game_Tick(&m_NoInput);
    Scene_Request(SCENE_NEXT_STAGE);
    CHECK(Game_Tick(&m_NoInput));
    CHECK_EQ_INT(Scene_Get(), SCENE_GAMEPLAY);
    CHECK_EQ_INT(g_SceneStubCalls.game_manager_remove, 1);
    CHECK_EQ_INT(g_SceneStubCalls.game_manager_register, 2);
}

TEST(quit_ends_the_tick_with_false)
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

TEST(a_switch_clears_the_buttons)
{
    M_Setup();
    Game_Tick(&m_NoInput); // boot to title

    TH06_INPUT input = {};
    input.shoot = true;
    Game_Tick(&input); // no switch this calc
    CHECK(g_Buttons.held.shoot);

    Scene_Request(SCENE_GAMEPLAY);
    Game_Tick(&input); // switches to gameplay
    CHECK(!g_Buttons.held.shoot);
    CHECK_EQ_INT(g_Buttons.word, 0);
}
