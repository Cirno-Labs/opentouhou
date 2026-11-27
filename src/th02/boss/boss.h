#pragma once

#include "th02/entity.h"
#include "th02/palette.h"
#include "th02/patterns.h"

#define BOSS_DEFEAT_INVINCIBILITY_FRAMES 200

// WAIT suspends the operation without advancing world clocks.
typedef enum {
    BOSS_WAIT = -1,
    BOSS_RUNNING = 1,
    BOSS_FINISHED = 2,
} BOSS_RESULT;

typedef struct {
    ENTITY_POINT position[2];
    int16_t phase_frame;
    int16_t damage;
    int16_t sprite;
    uint8_t defeated;
    uint8_t hit_flash;
    uint8_t angle;
} BOSS_STATE;

// Initializes a boss state, with the angle at 0x20.
void Boss_Init(BOSS_STATE *boss);

// Clears only the phase frame, damage, defeat and hit-flash flags.
void Boss_BeginStage(BOSS_STATE *boss);

// Clears a damage counter, then passes the replay hook the smallest damage
// that defeats the boss or advances its phase.
void Boss_ResetDamage(int16_t *damage, int16_t threshold);

// Returns the shot damage inside the box and adds it times multiplier to
// total, wrapping to a signed word.
int16_t Boss_Hit(
    int16_t *total, int16_t left, int16_t top, int16_t width, int16_t height,
    int16_t multiplier, uint8_t bomb_frame_mask);

// Copies bank's slots 128..511 into target, borrowing their pixels, and
// imports the optional palette without showing it.
void Boss_SelectPatterns(
    PATTERN_BANK *target, const PATTERN_BANK *bank, const GFX_COLOR *palette);
