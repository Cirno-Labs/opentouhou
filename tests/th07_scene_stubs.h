#pragma once

#include <stdint.h>

// Counts calls into the th07/stubs.h functions and GameManager_Register
// and GameManager_Remove, which this test links instead of src/th07/stubs.c
// and th07/game_manager's chain, so a test can check which targets the
// scene switch called. A test of the title defines TH07_TITLE_TEST, which
// leaves out the stand-in for TitleScene_SetUp.
typedef struct {
    int title_set_up;
    int game_manager_register;
    int game_manager_remove;
    int result_screen_register;
    int music_screen_register;
    int ending_register;
    int replay_discard;
    int32_t title_set_up_mode;
    // The scene that the switch left when TitleScene_SetUp was last called.
    int32_t title_set_up_previous;
    int32_t result_screen_register_mode;
    // The scene requested when Replay_Discard and GameManager_Register
    // were last called.
    int32_t replay_discard_scene;
    int32_t game_manager_register_scene;
    int play_time_add;
    int play_time_restart;
    int load_title_records;
    // Runs in ResultScreen_LoadTitleRecords, unless nullptr.
    void (*on_load_title_records)(void);
    // Makes Gui_IsLoadingShown return true.
    bool loading_shown;
} SCENE_STUB_CALLS;

extern SCENE_STUB_CALLS g_SceneStubCalls;

// Clears the counts, g_GameManager, g_Player, g_Gui and g_Stage, and sets
// g_EffectsQuality to its default.
void SceneStubCalls_Reset(void);
