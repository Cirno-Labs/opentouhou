#pragma once

// The falling pillars of stages 3 and 5: their startup, fall and the shaft
// the player can collide with.

#include "th02/entity.h"
#include "th02/patterns.h"

#define PILLAR_COUNT 12

typedef struct {
    uint8_t state;
    uint8_t phase;
    ENTITY_POINT position;
    int16_t frames;
    int16_t duration;
    uint8_t cel;
    uint8_t pattern;
} PILLAR;

typedef struct {
    PILLAR slots[PILLAR_COUNT];
    ENTITY_POINT current;
    uint8_t frames_new;
    bool enabled;
} PILLARS;

extern const PILLARS *const g_Pillars;

// Clears the pillars, once per run.
void Pillars_Init(void);

// Frees every slot at stage start, enabling pillars on stages 3 and 5;
// keeps payloads and the current position.
void Pillars_ResetStage(uint8_t stage);

// Enables the pillars, keeping their state.
void Pillars_Enable(void);

// Sets the frame count a new pillar starts with.
void Pillars_SetSpawnInterval(uint8_t frames);

// Adds a pillar; X outside 32..415 or a full pool adds nothing.
void Pillars_Add(int16_t left, int16_t top, int16_t duration, int16_t pattern);

// Invalidates the tiles under the pillars and frees removed slots; startup
// cels advance every fourth frame, removed or not.
void Pillars_Invalidate(PLAYFIELD_PAGES pages, uint32_t frame);

// Moves and draws the pillars; a full shaft hits the player under it, and
// the player module applies protection.
void Pillars_Update(
    PLAYFIELD_PAGES pages, uint32_t frame, const PATTERN_BANK *patterns);
