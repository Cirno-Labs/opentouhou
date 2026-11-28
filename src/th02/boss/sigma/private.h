#pragma once

#include "th02/boss/sigma.h"
#include "th02/boss/sigma/assets.h"
#include "th02/entity.h"

#include "core/base/types.h"

#define SIGMA_MIDBOSS_TILE_COUNT 30
#define SIGMA_CIRCLE_COUNT 16

// Which repeating attack a Sigma combat slot runs.
typedef enum {
    SIGMA_ATTACK_NONE,
    SIGMA_ATTACK_SWEEP,
    SIGMA_ATTACK_LOWER_CIRCLES,
    SIGMA_ATTACK_RING_DRIFT,
    SIGMA_ATTACK_ORBIT_CIRCLES,
    SIGMA_ATTACK_RAPID_DRIFT,
    SIGMA_ATTACK_BOUNCE_STREAM,
    SIGMA_ATTACK_PILLAR_FAN,
    SIGMA_ATTACK_TRACKING_CIRCLES,
    SIGMA_ATTACK_AIMED_CIRCLES,
    SIGMA_ATTACK_FAN_SWEEP,
    SIGMA_ATTACK_BALL_SWEEP,
    SIGMA_ATTACK_ALTERNATE_CIRCLES,
} SIGMA_ATTACK;

// The final boss's own fight phase, through its three combat rounds.
typedef enum {
    SIGMA_APPEAR,
    SIGMA_OPENING,
    SIGMA_PREPARE_ORBIT,
    SIGMA_ORBIT,
    SIGMA_PREPARE_BOUNCE,
    SIGMA_BOUNCE,
    SIGMA_PREPARE_TRACKING,
    SIGMA_TRACKING,
    SIGMA_PREPARE_FINAL,
    SIGMA_FINAL,
} SIGMA_PHASE;

// One of Sigma's expanding ring bullets.
typedef struct {
    uint8_t flag;
    uint8_t frame;
    ENTITY_POINT center;
    int16_t radius_max;
    uint8_t cel;
    uint8_t radius;
} SIGMA_CIRCLE;

// One tile of the stage 6 midboss's falling wall.
typedef struct {
    uint8_t flag;
    uint8_t frame;
    ENTITY_POINT position;
    uint8_t group;
    uint8_t angle;
    uint8_t speed;
    int8_t padding;
} SIGMA_MIDBOSS_TILE;

// Stage 6 boss Sigma's own state: her current phase, expanding circles
// and which three attacks the current combat round runs.
typedef struct {
    SIGMA_MIDBOSS_TILE tiles[SIGMA_MIDBOSS_TILE_COUNT];
    bool midboss_active;
    uint8_t midboss_spread;
    ENTITY_POINT position;
    uint8_t phase;
    uint8_t circle_frame_mask;
    SIGMA_CIRCLE circles[SIGMA_CIRCLE_COUNT];
    ENTITY_POINT bullet_origin;
    ENTITY_POINT orbit_center;
    ENTITY_POINT tracking_velocity;
    int16_t beam_left, beam_right, bounce_left, tracking_left, tracking_top,
        tracking_other_velocity, tracking_other_left, phase_damage;
    uint8_t beam_width, beam_byte_left, beam_byte_right, sweep_frames,
        orbit_angle, bounce_round, ball_angle, patterns_done, pattern,
        ring_angle;
    int8_t circle_hitbox_shrink, sweep_velocity, drift_velocity, pillar_index,
        wide_velocity;
    SIGMA_ATTACK attacks[3];
} SIGMA;

// Writable view of the module's private state, for tests.
extern SIGMA *const p_Sigma;

// Fills patterns with Sigma's bank and imports its palette, if assets
// names one.
void Sigma_SelectPatterns(PATTERN_BANK *patterns, const SIGMA_ASSETS *assets);

// Starts the fight: resets damage, position and the opening phase.
void Sigma_Begin(BOSS_STATE *boss);

// Draws Sigma's own two-sprite body.
void Sigma_DrawBody(const BOSS_STATE *boss, const PATTERN_BANK *patterns);

// Restores the scrolled-out tiles Sigma's sprite and rings marked dirty.
void Sigma_Invalidate(BOSS_STATE *boss);

// Advances Sigma one frame: moves, fires and checks for defeat.
BOSS_RESULT Sigma_Update(
    BOSS_STATE *boss, const PATTERN_BANK *patterns, bool reduce_effects);

// Runs the phase switch that starts each combat round and its closing
// patterns.
void Sigma_Phases(BOSS_STATE *boss);

// Advances the stage 6 midboss one frame.
void Sigma_MidbossUpdate(BOSS_STATE *boss);

// Reports whether the midboss is still active.
bool Sigma_MidbossInvalidate(void);
