#pragma once

#include "th01/scene.h"

typedef struct {
    const SCENE_DATA *active;
} SCENE_STATE;

// Writable view of the active scene for tests.
extern SCENE_STATE *const p_Scene;
