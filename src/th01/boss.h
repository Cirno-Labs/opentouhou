#pragma once

#include "core/vfs/types.h"
#include "pc98/zun/pi.h"
#include "th01/bos.h"
#include "th01/player.h"

typedef struct {
    int16_t hp, frame;
    int8_t phase;
} BOSS_STATE;

// Points to the active boss's own state, selected by its setup. The actor
// retains this storage across fights. Boss_Init clears the selection.
extern BOSS_STATE *g_Boss;

// Clears the active boss selection and debug damage for a new gameplay run.
void Boss_Init(void);

// Returns the value for the session difficulty, or normal for an unknown one.
int16_t Boss_Rank(int16_t easy, int16_t normal, int16_t hard, int16_t lunatic);

// Loads the named BOS file.
RESULT Boss_LoadBos(
    VFS *vfs, const char *name, const ALLOCATOR *allocator, BOS *out);

// Loads the named PI image.
RESULT Boss_LoadBackground(
    VFS *vfs, const char *name, const ALLOCATOR *allocator, PI_IMAGE *out);

// A debug request for the current boss to take one point of damage on its
// next hit test.
extern bool g_TestDamage;
