#pragma once

#include "th01/bos.h"
#include "th01/ptn.h"

#include "core/vfs/types.h"
#include "pc98/zun/pi.h"

typedef enum {
    BOSS_NONE,
    BOSS_SINGYOKU,
    BOSS_YUUGENMAGAN,
    BOSS_MIMA,
    BOSS_KIKURI,
    BOSS_ELIS,
    BOSS_SARIEL,
    BOSS_KONNGARA,
    BOSS_COUNT,
} BOSS;

typedef struct {
    int16_t hp;
    int16_t frame;
    int8_t phase;
} BOSS_STATE;

// Points to the active boss's own state, selected by its setup. The actor
// retains this storage across fights. Boss_Init clears the selection.
extern BOSS_STATE *const *const g_Boss;

// Clears the active boss selection and debug damage for a new gameplay run.
void Boss_Init(void);

// Selects the active boss by its own state.
void Boss_Select(BOSS_STATE *state);

// Clears the active boss selection, for a stage that has no boss.
void Boss_Deselect(void);

// Drops the debug damage request, once applied or at a life's end.
void Boss_ClearTestDamage(void);

// Returns the value for the session difficulty, or normal for an unknown one.
int16_t Boss_Rank(int16_t easy, int16_t normal, int16_t hard, int16_t lunatic);

// Loads the named BOS file.
RESULT BossAssets_LoadBos(
    VFS *vfs, const char *name, const ALLOCATOR *allocator, BOS *out);

// Loads the named PI image.
RESULT BossAssets_LoadBackground(
    VFS *vfs, const char *name, const ALLOCATOR *allocator, PI_IMAGE *out);

// Loads BOSS3_M.PTN, the missile sprites shared by boss3's two fights.
RESULT BossAssets_LoadMissiles(VFS *vfs, const ALLOCATOR *allocator, PTN *out);

// A debug request for the current boss to take one point of damage on its
// next hit test.
extern const bool *const g_TestDamage;
