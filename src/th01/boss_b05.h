#pragma once

#include "th01/boss_defeat.h"
#include "th01/boss_hit.h"
#include "th01/boss_render.h"
#include "th01/particles.h"
#include "th01/pellets.h"
#include "pc98/zun/pi.h"
#include "th01/route_select.h"

typedef struct {
    BOS sphere, flash, person;
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
    BOSS_ENTITY sphere, flash, person;
    BOSS_HIT hit;
    PALETTE boss_palette, post_defeat;
    BOSS_DEFEAT defeat;
    ROUTE_SELECT route_selection;
    int16_t hp, frame, pattern, pattern_state;
    int16_t velocity_x, velocity_y, rotation_interval;
    uint8_t angle;
    int8_t phase, direction;
    bool hp_bar_filled;
    B05_PENDING pending;
} B05;

extern B05 g_B05;

typedef struct {
    const B05_ASSETS *assets;
    SHOTS_OUTPUT shots_output;
    const PTN *miko;
    const ROUTE_SELECT_ASSETS *route;
} B05_CONTEXT;

// Loads three immutable BOS banks and BOSS1.GRP through VFS before ticks.
// Retains the background for the stage owner; the fight uses its palette.
// All storage belongs to the caller's grouped allocator. Failed loads leave
// out empty while the allocator retains any intermediate allocations.
RESULT B05_LoadAssets(VFS *vfs, const ALLOCATOR *allocator, B05_ASSETS *out);
RESULT B05_ValidateAssets(const B05_ASSETS *assets);

// Resets the boss state, before the first fight of a game.
void B05_Init(void);

// Binds validated sphere, flash and person assets to slots 0..2, shows their
// palette, and initializes the source's setup fields. Retains pattern history,
// hit timing, image locks and particle history until their specified entrance
// or pattern boundaries. Requires no pending operation. All contexts share
// palette and player. Setup allocates nothing, reads no file, and does not draw
// the background.
void B05_Setup(const B05_CONTEXT *context);

// Runs one ordinary boss call, or resumes its pending entrance/defeat/route
// operation. False suspends the owning world: advance only reference VSyncs
// and independent audio, then poll again. No prefix is replayed; the boss
// never advances player, Orb, pellets, shots or the world's frame_rand.
// Supply held raw input while pending; only the real route poll senses it.
// Entrance includes all 200 delays, then returns without an attack step.
// Death resets moving pellets and composes the real defeat and route
// modules, activating cached scene data before setting final flags.
//
// Retains the sphere's left-strip typo, slam overshoot, late crossing fire,
// male image 5 cache and transform overlays. Shared RNG is consumed before
// spawn attempts even when the pool is full. Keep contexts and assets bound
// identically until completion. frame_rand changes only between completed
// ordinary calls. Successful operations allocate nothing and perform no IO.
// After route completion, calls are inert until Setup or Init.
bool B05_UpdateAndRender(const B05_CONTEXT *context, const TH01_INPUT *raw);
