#pragma once

#include "th01/boss/private.h"
#include "th01/grc.h"
#include "th01/scene.h"
#include "th01/wait.h"

#include "pc98/zun/pi.h"

typedef struct {
    BOS girl;
    BOS attack;
    BOS bat;
    GRC spheres;
    PTN missiles;
    PI_IMAGE background;
    PALETTE palette;
    SCENE_DATA next_scene;
    const uint8_t *star;
} B15M_ASSETS;

typedef struct {
    int16_t x[5];
    int16_t y[5];
} B15M_RIFTS;

typedef struct {
    uint8_t angle;
    int16_t frame;
} B15M_CIRCLE;

typedef enum {
    B15M_PHASE_ENTRANCE,
    B15M_PHASE_PATTERNS_1,
    B15M_PHASE_CENTER,
    B15M_PHASE_PATTERNS_2,
    B15M_PHASE_TRANSFORM,
    B15M_PHASE_FORMS,
} B15M_PHASE;

typedef enum {
    B15M_FORM_GIRL,
    B15M_FORM_BAT,
} B15M_FORM;

typedef struct {
    BOSS_ENTITY girl;
    BOSS_ENTITY attack;
    BOSS_ENTITY bat;
    BOSS_HIT hit;
    PALETTE boss_palette;
    PALETTE post_defeat;
    BOSS_DEFEAT defeat;
    uint8_t background[128 * 96];
    B15M_RIFTS stars;
    B15M_RIFTS missile_rifts;
    B15M_RIFTS random_rifts;
    B15M_RIFTS girl_rifts;
    B15M_RIFTS bat_rifts;
    int16_t sphere_x[10];
    int16_t sphere_y[10];
    B15M_CIRCLE circle;
    B15M_CIRCLE star_circle;
    B15M_CIRCLE safety_circle;
    B15M_CIRCLE aimed_circle;
    BOSS_STATE status;
    int16_t pattern;
    int16_t pattern_state;
    int16_t form;
    int16_t circle_radius;
    int16_t direction;
    int16_t phase3_pattern;
    int16_t bat_target_x;
    int16_t bat_target_y;
    int16_t bat_target_frames;
    int16_t bat_velocity_x;
    int16_t bat_velocity_y;
    int16_t safety_left;
    int16_t girl_pattern;
    int16_t girl_subphase;
    int16_t phase5_subphase;
    int16_t bat_pattern;
    int16_t trail_x[2][3];
    int16_t trail_y[2][3];
    int16_t entrance_tick;
    int16_t entrance_delay;
    WAIT wait;
    uint8_t entrance_stage;
    uint8_t entrance_angle;
    uint8_t transform_stage;
    int8_t call_phase;
    bool teleport_done;
    bool hp_bar_filled;
    bool entrance_active;
    bool entrance_waiting;
    bool trails_offscreen;
    bool transform_bat;
    bool defeating;
    bool finished;
} B15M;

typedef enum {
    B15M_TRANSFORM_RIFTS = 1,
    B15M_TRANSFORM_SWAPPED = 2,
} B15M_TRANSFORM;

// Writable view of the b15m state for tests.
extern B15M *const p_B15M;

// Elis's lifecycle, for tests that look it up directly.
extern const BOSS_STRATEGY g_BossElis;

// Checks that assets loaded or faked for Elis are usable.
RESULT B15M_ValidateAssets(const B15M_ASSETS *a);
