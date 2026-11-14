#pragma once

#include <stdint.h>

// Counts calls into the th07/stubs.h functions this test links instead
// of src/th07/stubs.c, so a test can check which targets the scene
// switch called.
typedef struct {
    int title_set_up;
    int game_manager_register;
    int game_manager_remove;
    int result_screen_register;
    int music_screen_register;
    int ending_register;
    int replay_discard;
    int32_t title_set_up_mode;
    int32_t result_screen_register_mode;
    // The scene requested when Replay_Discard and GameManager_Register
    // were last called.
    int32_t replay_discard_scene;
    int32_t game_manager_register_scene;
} SCENE_STUB_CALLS;

extern SCENE_STUB_CALLS g_SceneStubCalls;

// Clears the counts and g_GameManager.
void SceneStubCalls_Reset(void);
