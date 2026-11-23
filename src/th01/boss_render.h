#pragma once

#include "th01/boss_entity.h"

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
    const BOSS_ENTITY *entity, const BOSS_SPRITES *bank, int16_t left,
    int16_t top, int16_t image);
void BossEntity_Erase(
    const BOSS_ENTITY *entity, const BOSS_SPRITES *bank, int16_t left,
    int16_t top, int16_t image);
void BossEntity_RestorePut(
    const BOSS_ENTITY *entity, const BOSS_SPRITES *bank, int16_t left,
    int16_t top, int16_t image);

// Requires row in 0..height-1. Restored lines rotate the mask word but shift
// signed RGB words arithmetically, retaining the source's edge artifacts.
void BossEntity_PutLine(
    const BOSS_ENTITY *entity, const BOSS_SPRITES *bank, int16_t left,
    int16_t top, int16_t image, int16_t row);
void BossEntity_RestorePutLine(
    const BOSS_ENTITY *entity, const BOSS_SPRITES *bank, int16_t left,
    int16_t top, int16_t image, int16_t row);

// Applies PutLine per sprite row with wrapped sine phase and coordinates.
// Requires a nonzero wavelength. Does not change entity position or image.
void BossEntity_WavePut(
    const BOSS_ENTITY *entity, const BOSS_SPRITES *bank, int16_t left,
    int16_t top, int16_t image, int16_t wavelength, int16_t amplitude,
    int16_t phase);

// Uses the normalized coverage of animation-bank assets without modifying
// their alpha. Unlike entity Put, ORs RGB even for transparent whole words.
void BossAnimation_Put(
    const BOSS_ANIMATION *animation, const BOSS_SPRITES *bank);

// Restores the whole actor rectangle through the existing EGC operation.
void BossEntity_EraseRect(const BOSS_ENTITY *entity);

// Moves/draws only at lock_frame 0, then sets it to 1. Later calls increment
// it or reset it at lock_frames, without drawing. With restore, restores the
// source's two old-edge strips before RestorePut; otherwise uses Put.
void BossEntity_LockedMovePut(
    BOSS_ENTITY *entity, const BOSS_SPRITES *bank, int16_t delta_x,
    int16_t delta_y, int16_t lock_frames, bool restore);
