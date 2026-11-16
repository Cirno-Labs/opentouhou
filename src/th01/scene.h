#pragma once

#include "th01/stage_data.h"

#include "core/vfs.h"

#define SCENE_COUNT 7

typedef struct {
    STAGE_DATA layouts;
    uint8_t id;
    char background[8], music[8];
} SCENE_DATA;

typedef struct {
    const SCENE_DATA *active;
} SCENE_STATE;

// Loads the five immutable layouts for scene 0..6 into the caller's grouped
// allocator. Derives ST{id}.GRP and ST{id}.MDT from id, ignoring header names.
// LoadReader keeps its reader open; Load opens STAGE{id}.DAT through VFS and
// closes that reader. Both leave out empty on failure. Load before ticks and
// retain the descriptor and its allocator while selected or cached.
RESULT Scene_LoadReader(
    READER *reader, uint8_t id, const ALLOCATOR *allocator, SCENE_DATA *out);
RESULT Scene_Load(
    VFS *vfs, uint8_t id, const ALLOCATOR *allocator, SCENE_DATA *out);

// Selects real cached layouts and their derived graphics/music names without
// reading files, rendering, resetting actors or allocating. The descriptor
// must have loaded successfully and remains borrowed. Re-selection is valid.
void Scene_Activate(SCENE_STATE *state, const SCENE_DATA *scene);
