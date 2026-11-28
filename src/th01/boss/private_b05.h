#pragma once

#include "th01/boss/private.h"

#include "pc98/zun/pi.h"

typedef struct {
    BOS sphere;
    BOS flash;
    BOS person;
    PI_IMAGE background;
    PALETTE palette;
} B05_ASSETS;

typedef enum {
    B05_IDLE,
    B05_ENTRANCE,
    B05_ENTRANCE_DELAY,
    B05_DEFEAT,
    B05_ROUTE,
    B05_FINISHED,
} B05_PENDING;

typedef enum {
    B05_PHASE_START = 0,
    B05_PHASE_SPHERE = 1,
    B05_PHASE_TRANSFORMS = 2,
    B05_PHASE_DEFEATED = 8,
} B05_PHASE;

typedef struct {
    BOSS_ENTITY sphere;
    BOSS_ENTITY flash;
    BOSS_ENTITY person;
    BOSS_HIT hit;
    PALETTE boss_palette;
    PALETTE post_defeat;
    BOSS_DEFEAT defeat;
    BOSS_STATE status;
    int16_t pattern;
    int16_t pattern_state;
    int16_t velocity_x;
    int16_t velocity_y;
    int16_t rotation_interval;
    uint8_t angle;
    int8_t direction;
    bool hp_bar_filled;
    B05_PENDING pending;
} B05;

typedef enum {
    B05_FIRE_NONE,
    B05_FIRE_DOWN,
    B05_FIRE_SLING,
} B05_FIRE;

// Writable view of the b05 state for tests.
extern B05 *const p_B05;

// SinGyoku's lifecycle, for tests that look it up directly.
extern const BOSS_STRATEGY g_BossSingyoku;

// Checks that assets loaded or faked for SinGyoku are usable.
RESULT B05_ValidateAssets(const B05_ASSETS *assets);
