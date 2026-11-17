#pragma once

#include "th01/boss_defeat.h"
#include "th01/boss_hit.h"
#include "th01/boss_render.h"
#include "th01/particles.h"
#include "th01/pellets.h"
#include "th01/pi.h"
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

typedef struct {
    const B05_ASSETS *assets;
    BOSS_SPRITES *sprites;
    const BOSS_HIT_CONTEXT *hit;
    PALETTE *stage_palette;
    PARTICLES *particles;
    PELLETS *pellets;
    const PELLET_SHARED *pellet_shared;
    uint32_t *rng;
    const PTN *miko;
    const ROUTE_SELECT_CONTEXT *route;
    void (*fade_music)(void *user);
} B05_CONTEXT;

// Loads three immutable BOS banks and BOSS1.GRP through VFS before ticks.
// Retains the background for the stage owner; the fight uses its palette.
// All storage belongs to the caller's grouped allocator. Failed loads leave
// out empty while the allocator retains any intermediate allocations.
RESULT B05_LoadAssets(VFS *vfs, const ALLOCATOR *allocator, B05_ASSETS *out);
RESULT B05_ValidateAssets(const B05_ASSETS *assets);

// Zero-initialize once per main program. Bind validated sphere, flash and
// person assets to shared slots 0..2, show their palette, and initialize the
// source's setup fields. Retains pattern history, hit timing, image locks and
// particle history until their specified entrance or pattern boundaries.
// Requires no pending operation. All contexts share video, palette and player.
// Setup allocates nothing, reads no file, and does not draw the background.
void B05_Setup(B05 *boss, const B05_CONTEXT *context);
