#pragma once

#include "th01/bomb.h"
#include "th01/boss.h"
#include "th01/hud.h"
#include "th01/hud_hp.h"
#include "th01/shots.h"

typedef struct {
    int16_t frame;
    bool invincible;
} BOSS_HIT;

typedef struct {
    int16_t left, top, width_minus_shot, height_minus_shot;
} BOSS_SHOT_HITBOX;

// Applies debug damage, then tests shots even during invincibility. Orb or
// bomb hits flash colors, decrement HP, reflect the Orb, add signed hit_score
// into unsigned score and render real HUD/health operations in order.
// Shots decay and emit effect 16 without damaging the boss. A boss hit posts
// effect 3.
//
// The owner advances frame externally. At frame 40 nothing resets; a value
// greater than 40 resets frame/invincibility and restores the boss palette,
// even if the boss was not invincible. Newly hit frame 0 flashes then restores
// that palette in the same call. Capture boss_palette separately from saved
// before battle; flashing changes saved and the DAC. Colors must be 0..15.
// Allocates nothing, performs no IO and uses loaded caller-owned resources.
void BossHit_UpdateAndRender(
    BOSS_HIT *hit, int16_t *hp, const PALETTE *boss_palette,
    const uint8_t *flash_colors, uint8_t flash_count, int16_t hit_score,
    bool orb_hit, BOSS_SHOT_HITBOX shot_hitbox);
