#pragma once

#include "th01/bos.h"
#include "th01/orb.h"

#define BOSS_ENTITY_SLOTS 4
#define BOSS_ANIMATION_SLOTS 2

typedef struct {
    const BOS *entities[BOSS_ENTITY_SLOTS];
    const BOS *animations[BOSS_ANIMATION_SLOTS];
} BOSS_SPRITES;

// Holds the BOS banks that the boss entities and animations draw from, by
// slot.
extern BOSS_SPRITES g_BossSprites;

typedef struct {
    int16_t width_bytes, height, count;
    uint8_t slot;
} BOSS_METADATA;

typedef struct {
    int16_t left, right, top, bottom;
} BOSS_BOUNDS;

typedef struct {
    int16_t left, top, previous_left, previous_top;
    BOSS_METADATA sprites;
    BOSS_BOUNDS move_clamp, orb_hitbox;
    int16_t image, lock_frame;
    bool orb_inactive;
} BOSS_ENTITY;

typedef struct {
    int16_t left, top;
    BOSS_METADATA sprites;
    uint8_t image;
} BOSS_ANIMATION;

// Borrows immutable, word-aligned BOS assets into a slot of g_BossSprites.
// Accepts one through eight images, heights 1..255 and widths 16..2032.
// Rebinding a slot changes its images for every entity that selects it,
// while existing entity metadata stays unchanged. Leaves all state
// unchanged on validation failure. Allocates nothing and performs no IO.
RESULT BossEntity_Bind(BOSS_ENTITY *entity, uint8_t slot, const BOS *sprites);
RESULT BossAnimation_Bind(
    BOSS_ANIMATION *animation, uint8_t slot, const BOS *sprites);

// Copies only dimensions, image count and slot; retains positions and image.
void BossEntity_CopyMetadata(BOSS_ENTITY *entity, const BOSS_ENTITY *source);

// Sets current position and clamps without moving or changing previous state.
// Clamp edges are applied separately during movement, even if reversed.
void BossEntity_SetPosition(
    BOSS_ENTITY *entity, int16_t left, int16_t top, BOSS_BOUNDS clamp);

// Adjusts relative edges by the Orb hitbox size. Activation stays unchanged.
void BossEntity_SetOrbHitbox(BOSS_ENTITY *entity, BOSS_BOUNDS bounds);

// Tests inclusive bounds against PREVIOUS Orb coordinates. Intermediate
// coordinate arithmetic wraps to signed 16 bits. Reads no external state.
bool BossEntity_HitTestOrb(const BOSS_ENTITY *entity);

// Saves previous position, adds signed deltas with 16-bit wrapping, then
// applies left/right and top/bottom clamps in that order. Draws nothing.
void BossEntity_Move(BOSS_ENTITY *entity, int16_t delta_x, int16_t delta_y);
