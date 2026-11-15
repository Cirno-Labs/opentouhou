#pragma once

#include "core/gfx.h"

#include <stdint.h>

// Counts calls into the th07/stubs.h functions this test links instead
// of src/th07/stubs.c, so a test can check which targets the scene
// switch called. A test of the title defines TH07_TITLE_TEST, which leaves
// out the stand-in for TitleScene_SetUp.
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
    int reroll_checksum_keys;
    int set_bombs;
    int32_t set_bombs_value;
    // The lives when GameManager_SetBombs was last called.
    float set_bombs_lives;
    int play_time_add;
    int play_time_restart;
    int pending_copies;
    int32_t copy_slot;
    GFX_RECT copy_source;
    GFX_RECT copy_dest;
    // Makes Anm_SetPendingCopy report a copy pending.
    bool copy_pending;
    int load_title_records;
    // Runs in ResultScreen_LoadTitleRecords, unless nullptr.
    void (*on_load_title_records)(void);
} SCENE_STUB_CALLS;

extern SCENE_STUB_CALLS g_SceneStubCalls;

// Clears the counts, g_GameManager, g_Player and g_Gui.
void SceneStubCalls_Reset(void);
