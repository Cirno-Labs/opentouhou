#pragma once

#include "th01/bomb.h"
#include "th01/boss.h"
#include "th01/boss/entity.h"
#include "th01/boss/strategy.h"
#include "th01/hud.h"
#include "th01/hud_hp.h"
#include "th01/palette.h"
#include "th01/player.h"
#include "th01/shots.h"
#include "th01/vsync_irq.h"

// Shares hit and defeat state among the files of the boss module.

typedef struct {
    int16_t frame;
    bool invincible;
} BOSS_HIT;

typedef struct {
    int16_t left;
    int16_t top;
    int16_t width_minus_shot;
    int16_t height_minus_shot;
} BOSS_SHOT_HITBOX;

typedef enum {
    BOSS_DEFEAT_REGULAR,
    BOSS_DEFEAT_SINGYOKU,
} BOSS_DEFEAT_KIND;

typedef enum {
    BOSS_DEFEAT_DONE,
    BOSS_DEFEAT_SCROLL_WAIT,
    BOSS_DEFEAT_WHITEOUT,
    BOSS_DEFEAT_WHITEOUT_DELAY,
    BOSS_DEFEAT_WHITEIN,
    BOSS_DEFEAT_WHITEIN_DELAY,
} BOSS_DEFEAT_PHASE;

typedef struct {
    BOSS_DEFEAT_PHASE phase;
    BOSS_DEFEAT_KIND kind;
    VSYNC_WAIT wait;
    int16_t top;
    int16_t bottom;
    int16_t frame;
} BOSS_DEFEAT;

// Applies debug damage and Orb or bomb hits: flashes, decrements HP, reflects
// the Orb and adds hit_score. Shots hit even during invincibility.
// Capture boss_palette before battle: flashing changes saved and the DAC.
void BossHit_UpdateAndRender(
    BOSS_HIT *hit, int16_t *hp, const PALETTE *boss_palette,
    const uint8_t *flash_colors, uint8_t flash_count, int16_t hit_score,
    bool orb_hit, BOSS_SHOT_HITBOX shot_hitbox);

RESULT BossDefeat_ValidateSprites(const PTN *miko);

// Starts the defeat sequence; begin only when the previous one is done.
// Keep the bound storage alive and the bindings unchanged until completion.
void BossDefeat_Begin(BOSS_DEFEAT *defeat, BOSS_DEFEAT_KIND kind);

// Resumes the defeat sequence; returns true when done. Regular completion
// sets stage_cleared and player.is_hit; SinGyoku leaves them to the caller,
// which sets them after the route chooser's scene starts.
bool BossDefeat_Resume(
    BOSS_DEFEAT *defeat, const PALETTE *post_defeat, const PTN *miko,
    const int16_t *origin_top);
