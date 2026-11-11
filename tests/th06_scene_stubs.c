#include "th06_scene_stubs.h"

#include "th06/screens/result.h"
#include "th06/screens/main.h"
#include "th06/screens/music.h"
#include "th06/screens/ending.h"
#include "th06/replay.h"

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
    g_SceneStubCalls.game_manager_register_scene = Scene_GetRequested();
    return OK;
}

void GameManager_Remove(void)
{
    g_SceneStubCalls.game_manager_remove++;
    g_SceneStubCalls.game_manager_remove_scene = Scene_GetRequested();
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
}

void ResultScreen_AddSpellCardAttempt(
    const int32_t index, const char *const name, const int32_t bonus)
{
    g_SceneStubCalls.spell_card_attempt++;
    g_SceneStubCalls.spell_card_attempt_index = index;
    g_SceneStubCalls.spell_card_attempt_name = name;
    g_SceneStubCalls.spell_card_attempt_bonus = bonus;
}

void ResultScreen_AddSpellCardCapture(
    const int32_t index, const int32_t shot_type)
{
    g_SceneStubCalls.spell_card_capture++;
    g_SceneStubCalls.spell_card_capture_index = index;
    g_SceneStubCalls.spell_card_capture_shot_type = shot_type;
}
