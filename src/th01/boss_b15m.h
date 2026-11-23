#pragma once

#include "th01/boss_defeat.h"
#include "th01/boss_hit.h"
#include "th01/boss_render.h"
#include "th01/missiles.h"
#include "th01/particles.h"
#include "th01/pellets.h"
#include "pc98/zun/pi.h"
#include "th01/scene.h"
#include "th01/shootout_lasers.h"

typedef struct {
    BOS girl, attack, bat;
    GRC spheres;
    PTN missiles;
    PI_IMAGE background;
    PALETTE palette;
    SCENE_DATA next_scene;
    const uint8_t *star;
} B15M_ASSETS;

typedef struct {
    int16_t x[5], y[5];
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

typedef enum {
    B15M_PENDING_NONE,
    B15M_PENDING_SCROLL,
    B15M_PENDING_SAFETY,
} B15M_PENDING;

typedef struct {
    BOSS_ENTITY girl, attack, bat;
    BOSS_HIT hit;
    PALETTE boss_palette, post_defeat;
    BOSS_DEFEAT defeat;
    uint8_t background[128 * 96];
    B15M_RIFTS stars, missile_rifts, random_rifts, girl_rifts, bat_rifts;
    int16_t sphere_x[10], sphere_y[10];
    B15M_CIRCLE circle, star_circle, safety_circle, aimed_circle;
    int16_t hp, frame, pattern, pattern_state, form;
    int16_t circle_radius, direction, phase3_pattern;
    int16_t bat_target_x, bat_target_y, bat_target_frames;
    int16_t bat_velocity_x, bat_velocity_y, safety_left;
    int16_t girl_pattern, girl_subphase, phase5_subphase, bat_pattern;
    int16_t trail_x[2][3], trail_y[2][3];
    int16_t entrance_tick, pending_scroll;
    FRAME_WAIT wait;
    uint64_t pending_tick;
    uint8_t entrance_stage, entrance_angle, pending, transform_stage;
    int8_t phase, call_phase;
    bool teleport_done, hp_bar_filled;
    bool entrance_active, entrance_waiting, trails_offscreen;
    bool transform_bat, defeating, finished;
} B15M;

extern B15M g_B15M;

typedef struct {
    const B15M_ASSETS *assets;
    const BOSS_HIT_CONTEXT *hit;
    const PTN *miko;
    bool *stage_cleared;
} B15M_CONTEXT;

// Loads the three Elis BOS banks, sphere GRC, missiles, background, scene 5
// and the star mask of shape8x8.bmp into the caller's asset group. Failed
// loads leave out empty; intermediate storage belongs to the group.
RESULT B15M_LoadAssets(VFS *vfs, const ALLOCATOR *allocator, B15M_ASSETS *out);
RESULT B15M_ValidateAssets(const B15M_ASSETS *assets);

// Resets the boss state, before the first fight of a game.
void B15M_Init(void);

// Binds banks, positions and hitboxes, resets HP and seeds RNG from the hit
// context's frame_rand. Composes load's pellet interlace and particle reset;
// retains missile, actor, hit and pattern history. The background owner
// applies its palette before entering. Allocates nothing and performs no IO.
void B15M_Setup(const B15M_CONTEXT *context);

// Runs one ordinary call including the missile prefix. False suspends its
// world during entrance, raw scroll, safety-circle delay or defeat. Advance
// independent audio and reference VSyncs and supply a monotonic 60 Hz host
// serial while polling. Polls never repeat prefixes, RNG or previous effects.
// Retains byte-aligned background snapshots, wasted teleport RNG draws,
// bat hitbox quirks and all ten broken laser resets; activates cached scene 5.
// Skips six entrance restores that read uninitialized source trail history.
// Uses initialized history thereafter. Does not model those undefined pixels
// or CPU drawing cost. Keeps borrowed bindings stable; allocates nothing and
// performs no IO. Finished calls remain inert until Setup or Init.
bool B15M_UpdateAndRender(const B15M_CONTEXT *context, uint64_t host_tick);
