#pragma once

#include "pc98/zun/math16.h"
#include "th01/boss_hit.h"
#include "th01/boss_entity.h"
#include "th01/grz.h"
#include "pc98/zun/pi.h"
#include "th01/pellets.h"
#include "th01/shootout_lasers.h"
#include "th01/stage_objects.h"
#include "th01/tram.h"

typedef struct {
    BOS head, face, aim;
    GRZ body;
    PI_IMAGE scroll, upper, layers[4];
    const uint8_t *diamond, *flake;
} B20J_ASSETS;

typedef struct {
    int16_t left[4][5], top[4][5];
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
    BOSS_ENTITY head, face, aim;
    BOSS_HIT hit;
    PALETTE boss_palette, stage_palette;
    B20J_SNAKES two_snakes, four_snakes;
    MATH16_VECTOR diamond_velocity[2];
    int16_t diamond_left[4], diamond_top[4], diamond_edges_frame;
    MATH16_VECTOR row_velocity;
    int16_t row_left, row_top, row_direction, row_speed;
    int16_t spray_delta, spray_fired;
    int16_t ray_x, ray_y, ray_previous_x, ray_previous_y;
    int16_t slash_left[3], slash_top[3];
    int16_t hp, frame, pattern_state, pattern, pattern_previous, patterns_done;
    int16_t face_direction, face_expression;
    int16_t scroll_line, scroll_speed, scroll_quarter, scroll_frame;
    int16_t animation_frame, raw_scroll;
    FRAME_WAIT wait;
    int8_t phase;
    uint8_t cup_angle, spray_offset, spray_angle;
    uint16_t delay_frames;
    uint8_t entrance_stage, defeat_stage, wait_kind;
    bool direction_can_change, laser_right_to_left, hp_bar_filled;
    bool entrance_active, defeating, finished;
} B20J;

extern B20J g_B20J;

// Loads all Konngara images and the shape masks of shape8x8.bmp into the
// caller's asset group. Failed loads leave out empty.
RESULT B20J_LoadAssets(VFS *vfs, const ALLOCATOR *allocator, B20J_ASSETS *out);
RESULT B20J_ValidateAssets(const B20J_ASSETS *assets);

// Resets the boss state, before the first fight of a game. Setup keeps the
// pattern history.
void B20J_Init(void);

// Resets HP, phase and face state and captures the current palette for hits.
// Runs at stage load, before the entrance.
void B20J_Setup(const B20J_ASSETS *assets);

// Starts the cached load sequence on the current drawing page, binds all
// three actors, snapshots its scroll palette and invokes the real empty
// boss-stage initializer.
void B20J_BeginEntrance(const B20J_ASSETS *assets);

// Suspends the world until all scroll, shake, pan and palette waits finish.
// Advance reference VSync counters while polling. Pending polls repeat no
// effects or drawing. Allocates nothing; no IO.
bool B20J_ResumeEntrance(const B20J_ASSETS *assets);

// Runs one ordinary battle call, or resumes the custom final defeat.
// False suspends the world until subsequent reference VSyncs satisfy waits.
// Victory draws all nine kuji images, finishes the final scroll, adds 50,000
// points and sets game_cleared. Finished calls stay inert. No Scene activation,
// allocations, IO or device reads occur.
bool B20J_UpdateAndRender(const B20J_ASSETS *assets);
