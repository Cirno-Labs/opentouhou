#pragma once

#include "pc98/zun/math16.h"
#include "th01/boss_hit.h"
#include "th01/boss_render.h"
#include "th01/grz.h"
#include "pc98/mdrv_data.h"
#include "th01/pi.h"
#include "th01/pellets.h"
#include "th01/reiiden_image.h"
#include "th01/shootout_lasers.h"
#include "th01/stage_objects.h"
#include "th01/tram.h"

typedef struct {
    BOS head, face, aim;
    GRZ body;
    PI_IMAGE scroll, upper, layers[4];
    MDRV_MUSIC music;
    MDRV_EFFECTS effects;
    const uint8_t *diamond, *flake;
} B20J_ASSETS;

typedef enum {
    B20J_AUDIO_LOAD_MUSIC,
    B20J_AUDIO_LOAD_EFFECTS,
    B20J_AUDIO_PLAY,
    B20J_AUDIO_STOP,
    B20J_AUDIO_FADE,
} B20J_AUDIO_ACTION;

typedef struct {
    int16_t left[4][5], top[4][5];
    MATH16_VECTOR velocity[4];
    int16_t target_left[4];
    bool target_locked[4];
} B20J_SNAKES;

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

typedef struct {
    const B20J_ASSETS *assets;
    BOSS_SPRITES *sprites;
    const BOSS_HIT_CONTEXT *hit;
    PELLETS *pellets;
    PELLET_SHARED *pellet_shared;
    SHOOTOUT_LASERS *lasers;
    STAGE_OBJECTS *objects;
    TRAM *tram;
    uint32_t *rng;
    bool *game_cleared;
    void (*audio)(
        void *user, B20J_AUDIO_ACTION action, const B20J_ASSETS *assets);
} B20J_CONTEXT;

// Loads all Konngara images, music, effects and verified executable shape
// masks into the caller's asset group. Scratch requires REIIDEN_IMAGE_SIZE
// bytes and can be released afterward. Failed loads leave out empty.
RESULT B20J_LoadAssets(
    VFS *vfs, uint8_t *scratch, size_t scratch_size, const ALLOCATOR *allocator,
    B20J_ASSETS *out);
RESULT B20J_ValidateAssets(const B20J_ASSETS *assets);

// Zero-initialize once per program. Preserves per-pattern history across
// setup calls. The caller's groups own all resources; no individual frees.
void B20J_Init(B20J *boss);

// Resets HP, phase and face state and captures the current palette for hits.
// Runs at stage load, before the entrance.
void B20J_Setup(B20J *boss, const B20J_CONTEXT *context);

RESULT B20J_ValidateContext(const B20J_CONTEXT *context);

// Starts the cached load sequence on the current drawing page, binds all
// three actors, snapshots its scroll palette and invokes the real empty
// boss-stage initializer.
// Required audio consumers are synchronous, nonblocking and allocate nothing.
void B20J_BeginEntrance(B20J *boss, const B20J_CONTEXT *context);

// Suspends the world until all scroll, shake, pan and palette waits finish.
// Advance reference VSync counters while polling. Pending polls repeat no
// effects or drawing. Allocates nothing; no IO.
bool B20J_ResumeEntrance(
    B20J *boss, const B20J_CONTEXT *context, FRAME_VSYNC *vsync);

// Runs one ordinary battle call, or resumes the custom final defeat.
// False suspends the world until subsequent reference VSyncs satisfy waits.
// Victory draws all nine kuji images, finishes the final scroll, adds 50,000
// points and sets game_cleared. Finished calls stay inert. No Scene activation,
// allocations, IO or device reads occur. Audio/effect consumers must not
// mutate borrowed boss, palette, score, player or projectile state.
bool B20J_UpdateAndRender(
    B20J *boss, const B20J_CONTEXT *context, FRAME_VSYNC *vsync);
