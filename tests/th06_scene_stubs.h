#pragma once

#include <stdint.h>

// Counts calls into the th06/stubs.h functions this test links instead
// of src/th06/stubs.c, so a test can check which targets the scene
// switch called.
typedef struct {
    int title_set_up;
    int game_manager_register;
    int game_manager_remove;
    int result_screen_register;
    int music_room_register;
    int ending_register;
    int replay_save;
    int32_t title_set_up_mode;
    int32_t result_screen_register_mode;
} SCENE_STUB_CALLS;

extern SCENE_STUB_CALLS g_SceneStubCalls;

void SceneStubCalls_Reset(void);
