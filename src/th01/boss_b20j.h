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
