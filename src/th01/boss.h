#pragma once

#include "core/vfs/types.h"
#include "pc98/zun/pi.h"
#include "th01/bos.h"
#include "th01/boss_hit.h"

// Returns the value for the session difficulty, or normal for an unknown one.
int16_t Boss_Rank(int16_t easy, int16_t normal, int16_t hard, int16_t lunatic);

// Loads the named BOS file.
RESULT Boss_LoadBos(
    VFS *vfs, const char *name, const ALLOCATOR *allocator, BOS *out);

// Loads the named PI image.
RESULT Boss_LoadBackground(
    VFS *vfs, const char *name, const ALLOCATOR *allocator, PI_IMAGE *out);

// Posts a sound effect through the hit context.
void Boss_Effect(const BOSS_HIT_CONTEXT *hit, int16_t number);

// Returns the player of the hit context.
PLAYER *Boss_Player(const BOSS_HIT_CONTEXT *hit);
