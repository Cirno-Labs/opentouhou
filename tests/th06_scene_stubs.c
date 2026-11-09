#include "th06_scene_stubs.h"

#include "th06/stubs.h"

SCENE_STUB_CALLS g_SceneStubCalls;

void SceneStubCalls_Reset(void)
{
    g_SceneStubCalls = (SCENE_STUB_CALLS) {};
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

RESULT MusicRoom_Register(void)
{
    g_SceneStubCalls.music_room_register++;
    return OK;
}

RESULT Ending_Register(void)
{
    g_SceneStubCalls.ending_register++;
    return OK;
}

void Replay_Save(void)
{
    g_SceneStubCalls.replay_save++;
}
