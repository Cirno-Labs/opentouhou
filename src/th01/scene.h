#pragma once

#include "th01/stage_data.h"
#include "th01/ptn.h"

#include "core/vfs.h"

#define SCENE_COUNT 7

typedef struct {
    STAGE_DATA layouts;
    uint8_t id;
    char background[8];
    char music[8];
} SCENE_DATA;

// Clears the selected scene for a new gameplay session.
void Scene_Init(void);

// Loads the five layouts of scene 0..6 into the caller's grouped allocator;
// LoadReader keeps its reader open, Load opens and closes STAGE{id}.DAT.
// Keep the descriptor and its allocator alive while selected or cached.
RESULT Scene_LoadReader(
    READER *reader, uint8_t id, const ALLOCATOR *allocator, SCENE_DATA *out);
RESULT Scene_Load(
    VFS *vfs, uint8_t id, const ALLOCATOR *allocator, SCENE_DATA *out);

// Selects a loaded scene's layouts and graphics and music names, without
// reading files or resetting actors.
void Scene_Activate(const SCENE_DATA *scene);

// Initializes prepared stage objects from the selected scene layouts.
void Scene_BeginStage(
    int16_t stage_id, bool first_in_scene, const PTN *sprites);
