#pragma once

#include "th01/boss_hit.h"
#include "th01/boss_entity.h"
#include "th01/grc.h"
#include "pc98/zun/pi.h"
#include "th01/pellets.h"
#include "th01/shootout_lasers.h"
#include "th01/stage_objects.h"
#include "th01/tram.h"
#include "th01/timer.h"
#include "th01/transition.h"

typedef struct {
    BOS shield, wand, dress;
    GRC vortex, bird, cross, splash;
    PI_IMAGE lower, upper, backgrounds[6];
    const INTERLEAVE_ASSETS *interleave;
    const uint8_t *star, *flake, *spark, *leaf_left, *leaf_right;
} B20M_ASSETS;

// Loads both forms, backgrounds and the masks of ileave_m.bmp,
// shape8x8.bmp, leaf_s.bmp, leaf_l.bmp and leaf_r.bmp into the caller's
// immutable asset group.
RESULT B20M_LoadAssets(VFS *vfs, const ALLOCATOR *allocator, B20M_ASSETS *out);
RESULT B20M_ValidateAssets(const B20M_ASSETS *assets);

// Resets the boss state, before the first fight of a game.
void B20M_Init(void);

// Binds actor images, resets the first form and retains pattern history.
// Run before the separate cached entrance. Performs no allocation or IO.
void B20M_Setup(const B20M_ASSETS *assets);

// Starts the cached load/scroll/diagonal entrance. Advance reference VSyncs
// and a monotonic 60 Hz host serial while polling; pending polls stay inert.
void B20M_BeginEntrance(const B20M_ASSETS *assets);
bool B20M_ResumeEntrance(const B20M_ASSETS *assets, uint64_t host_tick);

// Runs one battle call or resumes its nested waits. False suspends the world.
// Implements both forms and the final 50,000-point bonus and game clear.
// Quantizes the bird safety delay to six 60 Hz host ticks.
// Borrows stable assets and allocates nothing and performs no IO.
bool B20M_UpdateAndRender(const B20M_ASSETS *assets, uint64_t host_tick);
