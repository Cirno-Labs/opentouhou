#pragma once

#include "th01/bos.h"
#include "th01/orb.h"

#define BOSS_ENTITY_SLOTS 4
#define BOSS_ANIMATION_SLOTS 2

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

// Clears borrowed sprite banks for a new gameplay session.
void BossEntity_Init(void);

// Borrows immutable, word-aligned BOS assets into a shared rendering slot.
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

// Draws immutable shared-bank images with the actor's retained dimensions.
// Put rounds left toward zero to an eight-pixel byte; line operations use
// floor division and shifted source bytes. Keeps word guards, raw RGB OR,
// signed restore-line shifts and asymmetric edge clipping. Raw plane aliases
// are distinct from the 15-bit GRCG erase address. Hidden/non-VRAM bytes have
// no storage; a word crossing 65535 completes its physical byte accesses.
// Missing banks, stale dimensions that exceed storage, and invalid images
// draw nothing. Does not mutate assets, allocate, or perform IO.
// Put and PutLine preserve the selector. RestorePut selects 0 only when a
// word is reached; Erase finishes on 0 for valid images. Display stays fixed.
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

// Applies PutLine per sprite row with wrapped sine phase and coordinates.
// Requires a nonzero wavelength. Does not change entity position or image.
void BossEntity_WavePut(
    const BOSS_ENTITY *entity, int16_t left, int16_t top, int16_t image,
    int16_t wavelength, int16_t amplitude, int16_t phase);

// Uses the normalized coverage of animation-bank assets without modifying
// their alpha. Unlike entity Put, ORs RGB even for transparent whole words.
void BossAnimation_Put(const BOSS_ANIMATION *animation);

// Restores the whole actor rectangle through the existing EGC operation.
void BossEntity_EraseRect(const BOSS_ENTITY *entity);

// Moves/draws only at lock_frame 0, then sets it to 1. Later calls increment
// it or reset it at lock_frames, without drawing. With restore, restores the
// source's two old-edge strips before RestorePut; otherwise uses Put.
void BossEntity_LockedMovePut(
    BOSS_ENTITY *entity, int16_t delta_x, int16_t delta_y, int16_t lock_frames,
    bool restore);
