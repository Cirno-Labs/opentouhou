#pragma once

#include "th02/entity.h"
#include "th02/patterns.h"

#define SHOT_COUNT 38

typedef struct {
    uint8_t state;
    uint8_t age;
    ENTITY_POINT position[2];
    ENTITY_POINT velocity;
    uint8_t sprite;
    uint8_t from_option;
} SHOT;

typedef struct {
    uint8_t cycle[2];
    uint8_t cooldown[2];
    uint8_t level;
    uint8_t parameter[2];
    uint8_t maximum[2];
    int8_t spread;
} SHOTS_WEAPON;

typedef struct {
    SHOT slots[SHOT_COUNT];
    uint8_t frames[SHOT_COUNT];
    SHOTS_WEAPON weapon;
    ENTITY_POINT target;
    int16_t unused_target_x;
    uint8_t fire_cycle;
    uint8_t option_active;
    int8_t option_frames_per_age;
} SHOTS;

extern const SHOTS *const g_Shots;

// Initializes the shots with a four-frame option aging period, once per
// run; resets the firing cycle, weapon parameters and target.
void Shots_Init(void);

// Frees every slot at once, without tile invalidation.
void Shots_FreeAll(void);

// Clears flags and option activity at stage start, resets spread and the
// per-stage maximums for type 0..2, and the target; type 2 shortens the
// option aging period.
void Shots_ResetStage(uint8_t type);

// Invalidates the tiles under the shots, then frees removed slots; only
// sprites>=124 with nonzero age use 32x32 footprints.
void Shots_Invalidate(PLAYFIELD_PAGES pages, bool reduce_effects);

// Runs and draws one shot frame, animating even when reduced effects skip
// drawing. Requires a nonzero option aging period.
void Shots_Update(
    PLAYFIELD_PAGES pages, bool reduce_effects, uint32_t frame,
    const PATTERN_BANK *patterns);
