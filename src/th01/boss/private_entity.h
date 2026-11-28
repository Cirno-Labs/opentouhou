#pragma once

#include "th01/boss/entity.h"

typedef struct {
    const BOS *entities[BOSS_ENTITY_SLOTS];
    const BOS *animations[BOSS_ANIMATION_SLOTS];
} BOSS_SPRITES;

// Writable view of the bound boss sprites for tests.
extern BOSS_SPRITES *const p_Sprites;
