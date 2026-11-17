#pragma once

#include "th01/boss_defeat.h"
#include "th01/boss_hit.h"
#include "th01/boss_render.h"
#include "th01/missiles.h"
#include "th01/particles.h"
#include "th01/pellets.h"
#include "th01/pi.h"
#include "th01/scene.h"
#include "th01/shootout_lasers.h"

typedef struct {
    BOS eyes;
    PTN missiles;
    PI_IMAGE background;
    PALETTE palette;
    SCENE_DATA next_scene;
} B10M_ASSETS;

typedef struct {
    int16_t x[5], y[5], radius;
    MATH_VECTOR center, velocity;
    int16_t phase, angle;
} B10M_PENTAGRAM;

typedef struct {
    BOSS_ENTITY eyes[5];
    BOSS_HIT hit;
    PALETTE boss_palette, post_defeat;
    BOSS_DEFEAT defeat;
    B10M_PENTAGRAM pentagram;
    int16_t hp, frame, pattern_interval;
    int16_t pattern_state, subphase, target_left, after_hit_frame;
    int8_t phase, iterations;
    uint8_t angle, angle_southeast;
    bool hp_bar_filled, defeating, finished;
} B10M;

typedef struct {
    const B10M_ASSETS *assets;
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
} B10M_CONTEXT;

// Loads immutable BOSS2.BOS, BOSS2.GRP, BOSS3_M.PTN and scene 3 through VFS
// into the caller's grouped allocator. Retains the usable background. Failed
// loads leave out empty; intermediate storage stays in the allocator.
RESULT B10M_LoadAssets(VFS *vfs, const ALLOCATOR *allocator, B10M_ASSETS *out);
RESULT B10M_ValidateAssets(const B10M_ASSETS *assets);

// Zero-initialize once per main program. Shares slot 0 among all five eyes,
// initializes the source setup fields and resets missile flags. Retains eye
// lock/previous-position history, hit timing and pattern history. Reads no
// files, allocates nothing, and leaves the background drawing to its owner.
void B10M_Setup(B10M *boss, const B10M_CONTEXT *context);
