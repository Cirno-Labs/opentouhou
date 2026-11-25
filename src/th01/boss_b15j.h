#pragma once

#include "th01/boss_defeat.h"
#include "th01/boss_hit.h"
#include "th01/boss_render.h"
#include "th01/particles.h"
#include "th01/pellets.h"
#include "pc98/zun/pi.h"
#include "th01/scene.h"
#include "th01/shootout_lasers.h"

typedef struct {
    BOS souls, tears;
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
    BOSS_ENTITY souls[2], tears[10];
    int8_t tear_frame[10];
    BOSS_HIT hit;
    PALETTE boss_palette, post_defeat;
    BOSS_DEFEAT defeat;
    int16_t hp, frame, pattern_state, subphase, patterns_done;
    int16_t spiral_distance, random_range, ring_radius;
    uint8_t spiral_angle, spiral_drift, four_angle;
    int8_t phase;
    uint8_t entrance_stage;
    bool hp_bar_filled, entrance_active, entrance_waiting;
    bool defeating, finished;
} B15J;

extern B15J g_B15J;

// Loads TAMASII.BOS, TAMASII2.BOS, TAMAYEN.PTN, BOSS4.GRP and scene 6
// into the caller's asset group. Retains the usable background and palette.
// Failed loads leave out empty; intermediate storage belongs to the group.
RESULT B15J_LoadAssets(VFS *vfs, const ALLOCATOR *allocator, B15J_ASSETS *out);
RESULT B15J_ValidateAssets(const B15J_ASSETS *assets);

// Resets the boss state, before the first fight of a game.
void B15J_Init(void);

// Composes sprite binding, tear-frame reset and pellet interlace from loading
// with the setup's HP, soul positions and black defeat palette. Keeps actor and
// pattern history. The background owner applies its palette before entering the
// battle. No allocation or IO.
void B15J_Setup(const B15J_ASSETS *assets, const PTN *miko);

// Runs one ordinary call. False suspends the world during the barn-door
// and ring entrance or regular defeat. Advance only independent audio and
// reference VSyncs before polling again. Pending polls preserve actor state.
// Restores default Player and Orb sprites without advancing them. Preserves
// the current-Orb upper hitbox, overlapping ripple strips, shared pattern
// intervals, four-laser cleanup and silent HP timeout. Activates cached
// scene 6 after defeat. Finished calls are inert until setup or state reset.
// Keep borrowed resources stable. Allocates nothing and performs no IO.
bool B15J_UpdateAndRender(
    const B15J_ASSETS *assets, const PTN *miko, const PTN *stage);
