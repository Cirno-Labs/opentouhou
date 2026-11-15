#include "th07_scene_stubs.h"

#include "th07/game_manager.h"
#include "th07/replay.h"
#include "th07/scene.h"
#include "th07/screens/main.h"
#include "th07/stage.h"
#include "th07/player.h"
#include "th07/stubs.h"

#include <string.h>

PLAYER g_Player;
GUI g_Gui;
uint8_t g_EffectsQuality;
SCENE_STUB_CALLS g_SceneStubCalls;

void SceneStubCalls_Reset(void)
{
    memset(&g_SceneStubCalls, 0, sizeof(g_SceneStubCalls));
    memset(&g_GameManager, 0, sizeof(g_GameManager));
    memset(&g_Player, 0, sizeof(g_Player));
    memset(&g_Gui, 0, sizeof(g_Gui));
    memset(&g_Stage, 0, sizeof(g_Stage));
    g_EffectsQuality = 2;
}

#ifndef TH07_TITLE_TEST
RESULT TitleScene_SetUp(const int32_t mode)
{
    g_SceneStubCalls.title_set_up++;
    g_SceneStubCalls.title_set_up_mode = mode;
    g_SceneStubCalls.title_set_up_previous = Scene_GetPrevious();
    return OK;
}
#endif

void ResultScreen_LoadTitleRecords(void)
{
    g_SceneStubCalls.load_title_records++;
    if (g_SceneStubCalls.on_load_title_records != nullptr) {
        g_SceneStubCalls.on_load_title_records();
    }
}

RESULT GameManager_Register(void)
{
    g_SceneStubCalls.game_manager_register++;
    g_SceneStubCalls.game_manager_register_scene = Scene_GetRequested();
    return OK;
}

void GameManager_Remove(void)
{
    g_SceneStubCalls.game_manager_remove++;
}

RESULT ResultScreen_Register(const int32_t mode)
{
    g_SceneStubCalls.result_screen_register++;
    g_SceneStubCalls.result_screen_register_mode = mode;
    return OK;
}

RESULT MusicScreen_Register(void)
{
    g_SceneStubCalls.music_screen_register++;
    return OK;
}

RESULT Ending_Register(void)
{
    g_SceneStubCalls.ending_register++;
    return OK;
}

void Replay_Discard(void)
{
    g_SceneStubCalls.replay_discard++;
    g_SceneStubCalls.replay_discard_scene = Scene_GetRequested();
}

void PlayTime_Add(void)
{
    g_SceneStubCalls.play_time_add++;
}

void PlayTime_Restart(void)
{
    g_SceneStubCalls.play_time_restart++;
}

bool Gui_IsLoadingShown(void)
{
    return g_SceneStubCalls.loading_shown;
}
