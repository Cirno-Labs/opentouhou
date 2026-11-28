#pragma once

#include "th01/boss/private.h"
#include "th01/scene.h"

#include "pc98/zun/pi.h"

typedef struct {
    uint8_t masks[2][8][4];
} B10J_PILLARS;

typedef struct {
    BOS still;
    BOS animation;
    PTN missiles;
    PI_IMAGE background;
    PALETTE palette;
    SCENE_DATA next_scene;
    B10J_PILLARS pillars;
} B10J_ASSETS;

typedef struct {
    uint8_t angle;
    int16_t radius;
} B10J_SQUARE;

typedef enum {
    B10J_PHASE_START,
    B10J_PHASE_FIRST,
    B10J_PHASE_SPREAD,
    B10J_PHASE_SECOND,
} B10J_PHASE;

typedef struct {
    BOSS_ENTITY still;
    BOSS_ENTITY animation;
    BOSS_HIT hit;
    PALETTE boss_palette;
    PALETTE post_defeat;
    PALETTE_FADE setup_fade;
    VSYNC_WAIT setup_wait;
    BOSS_DEFEAT defeat;
    uint8_t background[128 * 160];
    B10J_SQUARE squares[6];
    int16_t pillar_time[8];
    int16_t pillar_x[8];
    int16_t pillar_bottom[8];
    int16_t laser_x[4];
    int16_t laser_y[4];
    BOSS_STATE status;
    int16_t pattern;
    int16_t pattern_state;
    int16_t target_left;
    uint8_t hop;
    uint8_t direction;
    uint8_t missile_angle;
    uint8_t pellet_angle;
    uint8_t spread_interval;
    uint8_t spread_speed;
    bool meteor_active;
    bool hp_bar_filled;
    bool setup_active;
    bool entrance_active;
    bool entrance_waiting;
    bool defeating;
    bool finished;
} B10J;

// Writable view of the b10j state for tests.
extern B10J *const p_B10J;

// Mima's lifecycle, for tests that look it up directly.
extern const BOSS_STRATEGY g_BossMima;

// Checks that assets loaded or faked for Mima are usable.
RESULT B10J_ValidateAssets(const B10J_ASSETS *a);
