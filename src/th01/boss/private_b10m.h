#pragma once

#include "th01/boss/private.h"
#include "th01/scene.h"

#include "pc98/zun/math16.h"
#include "pc98/zun/pi.h"

typedef struct {
    BOS eyes;
    PTN missiles;
    PI_IMAGE background;
    PALETTE palette;
    SCENE_DATA next_scene;
} B10M_ASSETS;

typedef enum {
    B10M_STAR_WAIT,
    B10M_STAR_SPIN_1,
    B10M_STAR_REVERSE_1,
    B10M_STAR_SPIN_2,
    B10M_STAR_REVERSE_2,
    B10M_STAR_SPIN_3,
    B10M_STAR_SLAM,
    B10M_STAR_RETURN,
    B10M_STAR_GROW,
    B10M_STAR_GROWN_SLAM,
} B10M_STAR_PHASE;

typedef enum {
    B10M_PHASE_ENTRANCE,
    B10M_PHASE_LATERAL,
    B10M_PHASE_TO_MISSILES,
    B10M_PHASE_MISSILES,
    B10M_PHASE_TO_SPRAY,
    B10M_PHASE_SPRAY,
    B10M_PHASE_TO_MISSILES_CLOCK,
    B10M_PHASE_MISSILES_CLOCK,
    B10M_PHASE_TO_LASERS,
    B10M_PHASE_LASERS,
    B10M_PHASE_TO_STAR,
    B10M_PHASE_STAR_SPAWN,
    B10M_PHASE_STAR_RISE,
    B10M_PHASE_STAR_ATTACK,
} B10M_PHASE;

typedef struct {
    int16_t x[5];
    int16_t y[5];
    int16_t radius;
    MATH16_VECTOR center;
    MATH16_VECTOR velocity;
    int16_t phase;
    int16_t angle;
} B10M_PENTAGRAM;

typedef struct {
    BOSS_ENTITY eyes[5];
    BOSS_HIT hit;
    PALETTE boss_palette;
    PALETTE post_defeat;
    BOSS_DEFEAT defeat;
    B10M_PENTAGRAM pentagram;
    BOSS_STATE status;
    int16_t pattern_interval;
    int16_t pattern_state;
    int16_t subphase;
    int16_t target_left;
    int16_t after_hit_frame;
    int8_t iterations;
    uint8_t angle;
    uint8_t angle_southeast;
    bool hp_bar_filled;
    bool defeating;
    bool finished;
} B10M;

// Writable view of the b10m state for tests.
extern B10M *const p_B10M;

// Yuugenmagan's lifecycle, for tests that look it up directly.
extern const BOSS_STRATEGY g_BossYuugenMagan;

// Checks that assets loaded or faked for Yuugenmagan are usable.
RESULT B10M_ValidateAssets(const B10M_ASSETS *a);
