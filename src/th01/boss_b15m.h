#pragma once

#include "th01/boss_defeat.h"
#include "th01/boss_hit.h"
#include "th01/boss_entity.h"
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

// Loads the three Elis BOS banks, sphere GRC, missiles, background, scene 5
// and the star mask of shape8x8.bmp into the caller's asset group. Failed
// loads leave out empty; intermediate storage belongs to the group.
RESULT B15M_LoadAssets(VFS *vfs, const ALLOCATOR *allocator, B15M_ASSETS *out);
RESULT B15M_ValidateAssets(const B15M_ASSETS *assets);

// Resets the boss state, before the first fight of a game.
void B15M_Init(void);

// Binds banks, positions and hitboxes, resets HP and seeds RNG from the hit
// session's frame_rand. Composes load's pellet interlace and particle reset;
// retains missile, actor, hit and pattern history. The background owner
// applies its palette before entering. Allocates nothing and performs no IO.
void B15M_Setup(const B15M_ASSETS *assets, const PTN *miko);

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
bool B15M_UpdateAndRender(
    const B15M_ASSETS *assets, const PTN *miko, uint64_t host_tick);
