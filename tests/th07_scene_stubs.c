#include "th07_scene_stubs.h"

#include "th07/scene.h"
#include "th07/stubs.h"

#include <string.h>

GAME_MANAGER g_GameManager;
SCENE_STUB_CALLS g_SceneStubCalls;

void SceneStubCalls_Reset(void)
{
    memset(&g_SceneStubCalls, 0, sizeof(g_SceneStubCalls));
    memset(&g_GameManager, 0, sizeof(g_GameManager));
}

RESULT TitleScene_SetUp(const int32_t mode)
{
    g_SceneStubCalls.title_set_up++;
    g_SceneStubCalls.title_set_up_mode = mode;
    return OK;
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
