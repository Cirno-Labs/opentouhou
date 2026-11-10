#pragma once

#include "th06/scene.h"

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
    int spell_card_attempt;
    int spell_card_capture;
    int32_t title_set_up_mode;
    int32_t result_screen_register_mode;
    // Hold the arguments of the last spell card attempt and capture.
    int32_t spell_card_attempt_index;
    const char *spell_card_attempt_name;
    int32_t spell_card_attempt_bonus;
    int32_t spell_card_capture_index;
    int32_t spell_card_capture_shot_type;
    // Hold Scene_GetRequested at the last call of each.
    SCENE game_manager_register_scene;
    SCENE game_manager_remove_scene;
} SCENE_STUB_CALLS;

extern SCENE_STUB_CALLS g_SceneStubCalls;

void SceneStubCalls_Reset(void);
