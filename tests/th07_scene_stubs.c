#include "th07_scene_stubs.h"

#include "th07/scene.h"
#include "th07/screens/main.h"
#include "th07/stubs.h"

#include <string.h>

GAME_MANAGER g_GameManager;
PLAYER g_Player;
GUI g_Gui;
SCENE_STUB_CALLS g_SceneStubCalls;

void SceneStubCalls_Reset(void)
{
    memset(&g_SceneStubCalls, 0, sizeof(g_SceneStubCalls));
    memset(&g_GameManager, 0, sizeof(g_GameManager));
    memset(&g_Player, 0, sizeof(g_Player));
    memset(&g_Gui, 0, sizeof(g_Gui));
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

void GameManager_RerollChecksumKeys(void)
{
    g_SceneStubCalls.reroll_checksum_keys++;
}

void GameManager_SetBombs(const int32_t bombs)
{
    g_SceneStubCalls.set_bombs++;
    g_SceneStubCalls.set_bombs_value = bombs;
    g_SceneStubCalls.set_bombs_lives = g_GameManager.player.lives;
}

void PlayTime_Add(void)
{
    g_SceneStubCalls.play_time_add++;
}

void PlayTime_Restart(void)
{
    g_SceneStubCalls.play_time_restart++;
}

bool Anm_SetPendingCopy(
    const int32_t slot, const GFX_RECT source, const GFX_RECT dest)
{
    if (g_SceneStubCalls.copy_pending) {
        return false;
    }
    g_SceneStubCalls.pending_copies++;
    g_SceneStubCalls.copy_slot = slot;
    g_SceneStubCalls.copy_source = source;
    g_SceneStubCalls.copy_dest = dest;
    return true;
}
