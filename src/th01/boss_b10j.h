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
    uint8_t masks[2][8][4];
} B10J_PILLARS;

typedef struct {
    BOS still, animation;
    PTN missiles;
    PI_IMAGE background;
    PALETTE palette;
    SCENE_DATA next_scene;
    B10J_PILLARS pillars;
} B10J_ASSETS;

// Loads BOSS3_1.BOS, BOSS3_2.BOS, BOSS3.GRP, BOSS3_M.PTN, scene 4 and
// the pillar masks of pillar.bmp into the caller's asset group. Failed
// loads leave out empty; intermediate storage belongs to the group.
RESULT B10J_LoadAssets(VFS *vfs, const ALLOCATOR *allocator, B10J_ASSETS *out);
RESULT B10J_ValidateAssets(const B10J_ASSETS *assets);

// Resets the boss state, before the first fight of a game.
void B10J_Init(void);

// Binds the two banks and starts the setup's white-in. ResumeSetup applies
// sixteen raw VSync waits and palette steps; each ten-millisecond delay ends
// before the next VSync. Advances position, HP and particle setup in the call
// of the final step. Keep resources stable and advance independent audio and
// VSyncs while suspended. Begin with black pages; the owning stage draws the
// background afterward. Neither operation allocates or reads files or devices.
void B10J_BeginSetup(const B10J_ASSETS *assets, const PTN *miko);
bool B10J_ResumeSetup(const B10J_ASSETS *assets, const PTN *miko);

// Runs one ordinary call, including missile and particle prefixes. False
// suspends the owning world during the entrance or regular defeat; advance
// only independent audio and reference VSyncs before polling again. Pending
// polls do not repeat prefixes or age actors. Activates cached scene 4 after
// defeat. Preserves old square-corner spawn positions, hop palette resets,
// delayed HP gates, broken sprite strips and five-laser cleanup. Calls after
// completion are inert. Keep borrowed resources stable through every wait.
// Requires completed setup. Allocates nothing and performs no IO.
bool B10J_UpdateAndRender(const B10J_ASSETS *assets, const PTN *miko);
