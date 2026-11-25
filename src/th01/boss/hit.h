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

// Applies debug damage and Orb or bomb hits: flashes, decrements HP, reflects
// the Orb and adds hit_score. Shots hit even during invincibility.
// Capture boss_palette before battle: flashing changes saved and the DAC.
void BossHit_UpdateAndRender(
    BOSS_HIT *hit, int16_t *hp, const PALETTE *boss_palette,
    const uint8_t *flash_colors, uint8_t flash_count, int16_t hit_score,
    bool orb_hit, BOSS_SHOT_HITBOX shot_hitbox);
