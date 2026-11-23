#pragma once

#include "pc98/zun/math16.h"
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
    BOS eyes;
    PTN missiles;
    PI_IMAGE background;
    PALETTE palette;
    SCENE_DATA next_scene;
} B10M_ASSETS;

typedef enum {
    B10M_STAR_WAIT,
    B10M_STAR_SPIN_1,
    B10M_STAR_REVERSE_1,
    B10M_STAR_SPIN_2,
    B10M_STAR_REVERSE_2,
    B10M_STAR_SPIN_3,
    B10M_STAR_SLAM,
    B10M_STAR_RETURN,
    B10M_STAR_GROW,
    B10M_STAR_GROWN_SLAM,
} B10M_STAR_PHASE;

typedef enum {
    B10M_PHASE_ENTRANCE,
    B10M_PHASE_LATERAL,
    B10M_PHASE_TO_MISSILES,
    B10M_PHASE_MISSILES,
    B10M_PHASE_TO_SPRAY,
    B10M_PHASE_SPRAY,
    B10M_PHASE_TO_MISSILES_CLOCK,
    B10M_PHASE_MISSILES_CLOCK,
    B10M_PHASE_TO_LASERS,
    B10M_PHASE_LASERS,
    B10M_PHASE_TO_STAR,
    B10M_PHASE_STAR_SPAWN,
    B10M_PHASE_STAR_RISE,
    B10M_PHASE_STAR_ATTACK,
} B10M_PHASE;

typedef struct {
    int16_t x[5], y[5], radius;
    MATH16_VECTOR center, velocity;
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

extern B10M g_B10M;

typedef struct {
    const B10M_ASSETS *assets;
    const BOSS_HIT_CONTEXT *hit;
    PALETTE *stage_palette;
    PARTICLES *particles;
    MISSILES *missiles;
    SHOOTOUT_LASERS *lasers;
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

// Resets the boss state, before the first fight of a game.
void B10M_Init(void);

// Shares slot 0 among all five eyes, initializes the source setup fields and
// resets missile flags. Retains eye lock/previous-position history, hit
// timing and pattern history. Reads no files, allocates nothing, and leaves
// the background drawing to its owner.
void B10M_Setup(const B10M_CONTEXT *context);

// Runs one ordinary call including the missile prefix. The 331-frame entrance
// and eye transitions leave the owning world running. False suspends it during
// regular defeat: advance only reference VSyncs and independent audio before
// polling again. Pending polls never repeat the prefix or advance actors.
// Activates cached scene 3 after the real defeat sets clear/hit flags. Retains
// eye lock flicker, asymmetrical laser hits/restores, signed palette overflow,
// pentagram overshoot and phase fallthrough. Keep context bindings stable.
// Successful calls allocate nothing and perform no IO. Finished calls are inert
// until setup or the main program's state reset.
bool B10M_UpdateAndRender(const B10M_CONTEXT *context);
