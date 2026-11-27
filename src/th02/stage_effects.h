#pragma once

// Stage weather and transition effects: the storm sparks of stage 1, the
// rain of stage 2, the color vortex of stage 3, the portal reveal of
// stage 4, and the lightning flashes stages 1 and 2 share.

#include "th02/entity.h"

#include "core/base/types.h"

#define STAGE_PARTICLE_COUNT 50

typedef struct {
    ENTITY_STATE state;
    ENTITY_POINT position[2];
    int16_t speed;
} STAGE_PARTICLE;

typedef struct {
    STAGE_PARTICLE particles[STAGE_PARTICLE_COUNT];
    int16_t storm_frame;
    uint16_t rain_frame;
    uint16_t lightning_frame;
    uint16_t lightning_next;
    uint16_t vortex_frame;
    bool rain_clip_reset;
    int16_t radius;
    int16_t center_x;
    int16_t center_y;
    uint8_t angle;
    ENTITY_POINT dots[2][64];
    uint16_t portal_top;
    uint8_t stage;
} STAGE_EFFECTS;

extern const STAGE_EFFECTS *const g_StageEffects;

// Initializes weather and particles, once per run.
void StageEffects_Init(void);

// Resets the vortex timer and selects the stage's effect at stage entry.
void StageEffects_ResetStage(uint8_t stage);

// Invalidates the tiles under the weather particles.
void StageEffects_Invalidate(PLAYFIELD_PAGES pages);

// Runs one effects frame on the back page; requires tile sources and image
// or disabled tile mode.
void StageEffects_Update(
    PLAYFIELD_PAGES pages, uint8_t stage, bool reduce_effects,
    uint8_t *slowdown);

// Starts the storm sparks (stage 1's entrance).
void StageEffects_StartStorm(void);

// Starts the shared lightning flashes.
void StageEffects_StartLightning(void);

// Stops the shared lightning flashes.
void StageEffects_StopLightning(void);
