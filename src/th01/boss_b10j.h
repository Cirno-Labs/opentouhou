#pragma once

#include "th01/boss_defeat.h"
#include "th01/boss_hit.h"
#include "th01/boss_render.h"
#include "th01/missiles.h"
#include "th01/particles.h"
#include "th01/pellets.h"
#include "th01/pi.h"
#include "th01/reiiden_image.h"
#include "th01/scene.h"
#include "th01/shootout_lasers.h"

typedef struct {
    uint8_t masks[2][8][4];
} B10J_PILLARS;

typedef struct {
    BOS still, animation;
    PTN missiles;
    PI_IMAGE background;
    PALETTE palette;
    SCENE_DATA next_scene;
    const B10J_PILLARS *pillars;
} B10J_ASSETS;

typedef struct {
    uint8_t angle;
    int16_t radius;
} B10J_SQUARE;

typedef struct {
    BOSS_ENTITY still, animation;
    BOSS_HIT hit;
    PALETTE boss_palette, post_defeat;
    PALETTE_FADE setup_fade;
    FRAME_WAIT setup_wait;
    uint64_t setup_tick;
    BOSS_DEFEAT defeat;
    uint8_t *background;
    B10J_SQUARE squares[6];
    int16_t pillar_time[8], pillar_x[8], pillar_bottom[8];
    int16_t laser_x[4], laser_y[4];
    int16_t hp, frame, pattern, pattern_state, target_left;
    int8_t phase;
    uint8_t hop, direction, missile_angle, pellet_angle;
    uint8_t spread_interval, spread_speed;
    bool meteor_active, hp_bar_filled;
    bool setup_active, setup_waiting, entrance_active, entrance_waiting;
    bool defeating, finished;
} B10J;

typedef struct {
    const B10J_ASSETS *assets;
    BOSS_SPRITES *sprites;
    const BOSS_HIT_CONTEXT *hit;
    PALETTE *stage_palette;
    PARTICLES *particles;
    PELLETS *pellets;
    PELLET_SHARED *pellet_shared;
    MISSILES *missiles;
    SHOOTOUT_LASERS *lasers;
    uint32_t *rng;
    const PTN *miko;
    SCENE_STATE *scene;
    bool *stage_cleared;
    void (*fade_music)(void *user);
} B10J_CONTEXT;

// Loads BOSS3_1.BOS, BOSS3_2.BOS, BOSS3.GRP, BOSS3_M.PTN, scene 4 and
// the verified REIIDEN.EXE pillar masks into the caller's asset group.
// Scratch requires REIIDEN_IMAGE_SIZE bytes and may be released afterward.
// Failed loads leave out empty; intermediate storage belongs to the group.
RESULT B10J_LoadAssets(
    VFS *vfs, uint8_t *scratch, size_t scratch_size, const ALLOCATOR *allocator,
    B10J_ASSETS *out);
RESULT B10J_ValidateAssets(const B10J_ASSETS *assets);

// Initializes one main program and reserves the mutable sprite background
// from its allocator. Reset the group together with the owning program.
void B10J_Init(B10J *boss, const ALLOCATOR *allocator);

// Binds the two banks and starts the setup's white-in. ResumeSetup applies
// sixteen raw VSync waits, palette steps and ten-millisecond delays.
// Advances position, HP and particle setup only after the final delay.
// Keep resources stable and advance independent audio and VSyncs while
// suspended. Supply a monotonic 60 Hz host serial for the ten-ms waits;
// visibility is quantized to host ticks without modeling CPU drawing cost.
// Begin with black pages; the owning stage draws the background afterward.
// Neither operation allocates or reads files or devices.
void B10J_BeginSetup(
    B10J *boss, const B10J_CONTEXT *context, const FRAME_VSYNC *vsync);
bool B10J_ResumeSetup(
    B10J *boss, const B10J_CONTEXT *context, const FRAME_VSYNC *vsync,
    uint64_t host_tick);

RESULT B10J_ValidateContext(const B10J_CONTEXT *context);

// Runs one ordinary call, including missile and particle prefixes. False
// suspends the owning world during the entrance or regular defeat; advance
// only independent audio and reference VSyncs before polling again. Pending
// polls do not repeat prefixes or age actors. Activates cached scene 4 after
// defeat. Preserves old square-corner spawn positions, hop palette resets,
// delayed HP gates, broken sprite strips and five-laser cleanup. Calls after
// completion are inert. Keep borrowed resources stable through every wait.
// Requires completed setup. Allocates nothing and performs no IO.
bool B10J_UpdateAndRender(
    B10J *boss, const B10J_CONTEXT *context, FRAME_VSYNC *vsync);
