#pragma once

#include "th01/boss/private.h"
#include "th01/scene.h"

#include "pc98/zun/pi.h"

typedef struct {
    BOS souls;
    BOS tears;
    PTN ripples;
    PI_IMAGE background;
    PALETTE palette;
    SCENE_DATA next_scene;
} B15J_ASSETS;

typedef enum {
    B15J_PHASE_ENTRANCE = 0,
    B15J_PHASE_SPIRAL = 2,
    B15J_PHASE_RECOLOR = 3,
    B15J_PHASE_SOULS = 4,
    B15J_PHASE_TEARS = 5,
    B15J_PHASE_PATTERNS = 6,
} B15J_PHASE;

typedef struct {
    BOSS_ENTITY souls[2];
    BOSS_ENTITY tears[10];
    int8_t tear_frame[10];
    BOSS_HIT hit;
    PALETTE boss_palette;
    PALETTE post_defeat;
    BOSS_DEFEAT defeat;
    BOSS_STATE status;
    int16_t pattern_state;
    int16_t subphase;
    int16_t patterns_done;
    int16_t spiral_distance;
    int16_t random_range;
    int16_t ring_radius;
    uint8_t spiral_angle;
    uint8_t spiral_drift;
    uint8_t four_angle;
    uint8_t entrance_stage;
    bool hp_bar_filled;
    bool entrance_active;
    bool entrance_waiting;
    bool defeating;
    bool finished;
} B15J;

// Writable view of the b15j state for tests.
extern B15J *const p_B15J;

// Kikuri's lifecycle, for tests that look it up directly.
extern const BOSS_STRATEGY g_BossKikuri;

// Checks that assets loaded or faked for Kikuri are usable.
RESULT B15J_ValidateAssets(const B15J_ASSETS *a);

// Loads Kikuri's assets, for tests that drive it directly.
RESULT B15J_LoadAssets(VFS *vfs, const ALLOCATOR *allocator, void *data);
