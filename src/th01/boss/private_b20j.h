#pragma once

#include "th01/boss/private.h"
#include "th01/grz.h"
#include "th01/wait.h"

#include "pc98/zun/math16.h"
#include "pc98/zun/pi.h"

typedef struct {
    BOS head;
    BOS face;
    BOS aim;
    GRZ body;
    PI_IMAGE scroll;
    PI_IMAGE upper;
    PI_IMAGE layers[4];
    const uint8_t *diamond;
    const uint8_t *flake;
} B20J_ASSETS;

typedef struct {
    int16_t left[4][5];
    int16_t top[4][5];
    MATH16_VECTOR velocity[4];
    int16_t target_left[4];
    bool target_locked[4];
} B20J_SNAKES;

typedef enum {
    B20J_PHASE_START,
    B20J_PHASE_PATTERNS_1,
    B20J_PHASE_FLASH_1,
    B20J_PHASE_PATTERNS_2,
    B20J_PHASE_FLASH_2,
    B20J_PHASE_PATTERNS_3,
    B20J_PHASE_FLASH_3,
    B20J_PHASE_FINAL,
} B20J_PHASE;

typedef enum {
    B20J_ENTRANCE_SCROLL = 1,
    B20J_ENTRANCE_SCROLL_STEP,
    B20J_ENTRANCE_SCROLL_END,
    B20J_ENTRANCE_SHAKE_DELAY,
    B20J_ENTRANCE_SHAKE,
    B20J_ENTRANCE_SHAKE_STEP,
    B20J_ENTRANCE_PAN,
    B20J_ENTRANCE_PAN_STEP,
    B20J_ENTRANCE_TREMBLE,
    B20J_ENTRANCE_TREMBLE_STEP,
    B20J_ENTRANCE_FLASH_DELAY,
    B20J_ENTRANCE_FLASH,
    B20J_ENTRANCE_FADE_IN,
} B20J_ENTRANCE;

typedef enum {
    B20J_DEFEAT_KUJI = 1,
    B20J_DEFEAT_SHAKE,
    B20J_DEFEAT_SHAKE_STEP,
    B20J_DEFEAT_SCROLL,
    B20J_DEFEAT_SCROLL_STEP,
    B20J_DEFEAT_CLEAR,
} B20J_DEFEAT;

typedef struct {
    BOSS_ENTITY head;
    BOSS_ENTITY face;
    BOSS_ENTITY aim;
    BOSS_HIT hit;
    PALETTE boss_palette;
    PALETTE stage_palette;
    B20J_SNAKES two_snakes;
    B20J_SNAKES four_snakes;
    MATH16_VECTOR diamond_velocity[2];
    int16_t diamond_left[4];
    int16_t diamond_top[4];
    int16_t diamond_edges_frame;
    MATH16_VECTOR row_velocity;
    int16_t row_left;
    int16_t row_top;
    int16_t row_direction;
    int16_t row_speed;
    int16_t spray_delta;
    int16_t spray_fired;
    int16_t ray_x;
    int16_t ray_y;
    int16_t ray_previous_x;
    int16_t ray_previous_y;
    int16_t slash_left[3];
    int16_t slash_top[3];
    BOSS_STATE status;
    int16_t pattern_state;
    int16_t pattern;
    int16_t pattern_previous;
    int16_t patterns_done;
    int16_t face_direction;
    int16_t face_expression;
    int16_t scroll_line;
    int16_t scroll_speed;
    int16_t scroll_quarter;
    int16_t scroll_frame;
    int16_t animation_frame;
    WAIT wait;
    uint8_t cup_angle;
    uint8_t spray_offset;
    uint8_t spray_angle;
    uint8_t entrance_stage;
    uint8_t defeat_stage;
    bool direction_can_change;
    bool laser_right_to_left;
    bool hp_bar_filled;
    bool entrance_active;
    bool defeating;
    bool finished;
} B20J;

// Writable view of the b20j state for tests.
extern B20J *const p_B20J;

// Konngara's lifecycle, for tests that look it up directly.
extern const BOSS_STRATEGY g_BossKonngara;

// Checks that assets loaded or faked for Konngara are usable.
RESULT B20J_ValidateAssets(const B20J_ASSETS *a);
