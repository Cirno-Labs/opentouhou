#pragma once

#include "th01/bos.h"
#include "th01/orb.h"

#define BOSS_ENTITY_SLOTS 4
#define BOSS_ANIMATION_SLOTS 2

typedef struct {
    int16_t width_bytes;
    int16_t height;
    int16_t count;
    uint8_t slot;
} BOSS_METADATA;

typedef struct {
    int16_t left;
    int16_t right;
    int16_t top;
    int16_t bottom;
} BOSS_BOUNDS;

typedef struct {
    int16_t left;
    int16_t top;
    int16_t previous_left;
    int16_t previous_top;
    BOSS_METADATA sprites;
    BOSS_BOUNDS move_clamp;
    BOSS_BOUNDS orb_hitbox;
    int16_t image;
    int16_t lock_frame;
    bool orb_inactive;
} BOSS_ENTITY;

typedef struct {
    int16_t left;
    int16_t top;
    BOSS_METADATA sprites;
    uint8_t image;
} BOSS_ANIMATION;

// Clears borrowed sprite banks for a new gameplay session.
void BossEntity_Init(void);

// Borrows word-aligned BOS assets into a shared slot; rebinding changes the
// images of every entity that selects it. Keep sprites alive while bound.
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

// Tests inclusive bounds against the previous Orb coordinates.
bool BossEntity_HitTestOrb(const BOSS_ENTITY *entity);

// Saves the previous position, adds the deltas and applies the clamps.
void BossEntity_Move(BOSS_ENTITY *entity, int16_t delta_x, int16_t delta_y);

// Draws the entity's image from its slot with its retained dimensions.
// Put rounds left toward zero to eight pixels; Erase restores page 0.
void BossEntity_Put(
    const BOSS_ENTITY *entity, int16_t left, int16_t top, int16_t image);
void BossEntity_Erase(
    const BOSS_ENTITY *entity, int16_t left, int16_t top, int16_t image);
void BossEntity_RestorePut(
    const BOSS_ENTITY *entity, int16_t left, int16_t top, int16_t image);

// Requires row in 0..height-1. Restored lines rotate the mask word but shift
// signed RGB words arithmetically, retaining the source's edge artifacts.
void BossEntity_PutLine(
    const BOSS_ENTITY *entity, int16_t left, int16_t top, int16_t image,
    int16_t row);
void BossEntity_RestorePutLine(
    const BOSS_ENTITY *entity, int16_t left, int16_t top, int16_t image,
    int16_t row);

// Applies PutLine per sprite row at a sine-displaced X; wavelength is nonzero.
void BossEntity_WavePut(
    const BOSS_ENTITY *entity, int16_t left, int16_t top, int16_t image,
    int16_t wavelength, int16_t amplitude, int16_t phase);

// Uses the normalized coverage of animation-bank assets without modifying
// their alpha. Unlike entity Put, ORs RGB even for transparent whole words.
void BossAnimation_Put(const BOSS_ANIMATION *animation);

// Restores the whole actor rectangle from page 1.
void BossEntity_EraseRect(const BOSS_ENTITY *entity);

// Moves and draws only at lock_frame 0, then counts up to lock_frames.
// With restore, restores the two old-edge strips and uses RestorePut.
void BossEntity_LockedMovePut(
    BOSS_ENTITY *entity, int16_t delta_x, int16_t delta_y, int16_t lock_frames,
    bool restore);
