#pragma once

#include "th02/boss/rika.h"
#include "th02/entity.h"

#include "core/base/types.h"

// Rika's current sweep phase.
typedef enum {
    RIKA_SWEEP,
    RIKA_CENTER,
    RIKA_HOLD,
} RIKA_PHASE;

// Stage 1 boss Rika's own moving sprite: sweep angle, hold phase and the
// fire mask that staggers which bullets of a pattern actually fire.
typedef struct {
    ENTITY_POINT current;
    RIKA_PHASE phase;
    int16_t kill_frame;
    uint16_t frame;
    uint8_t angle;
    uint8_t first_pattern;
    uint8_t second_pattern;
    uint8_t mirrored_pattern;
    uint8_t fire_mask;
} RIKA;

// The stage 1 midboss that shares Rika's sprite bank.
typedef struct {
    int16_t kill_frame;
    int16_t sprite;
    bool active;
} RIKA_MIDBOSS;

// Writable view of the module's private state, for tests.
extern RIKA *const p_Rika;
extern RIKA_MIDBOSS *const p_RikaMidboss;

// Clears Rika's and the midboss's state, once per MAIN process.
void Rika_Init(void);

// Starts Rika's fight: resets damage, position and her fire patterns for
// rank.
void Rika_Begin(BOSS_STATE *boss, uint8_t rank);

// Advances Rika one frame: moves, fires and checks for defeat.
BOSS_RESULT Rika_Update(
    BOSS_STATE *boss, const PATTERN_BANK *patterns, uint8_t rank);

// Restores the scrolled-out tile row Rika's sprite marked dirty.
void Rika_Invalidate(BOSS_STATE *boss);

// Advances the stage 1 midboss one frame; shares Rika's sprite bank.
void Rika_MidbossUpdate(BOSS_STATE *boss, const PATTERN_BANK *patterns);
