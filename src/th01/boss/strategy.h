#pragma once

#include "th01/boss.h"
#include "th01/input.h"
#include "th01/route_select.h"

// The world state a boss borrows for one call.
typedef struct {
    const PTN *miko;
    const PTN *stage;
    const ROUTE_SELECT_ASSETS *route;
    const TH01_INPUT *raw;
    uint64_t host_tick;
} BOSS_CONTEXT;

// How a boss's defeat sequence plays out; BossDefeat_Begin takes one from
// the boss's own strategy. Regular completion sets stage_cleared and
// player.is_hit; SinGyoku's leaves them to the caller, which sets them after
// the route chooser's scene starts.
typedef struct {
    // Offset from origin_top the whiteout stripes start at.
    int16_t shift;
    // Pixels the stripes move apart by, per whiteout frame.
    int16_t step;
    // Frames between a whiteout palette step.
    int16_t whiteout_period;
    // Frames between a whitein palette step.
    int16_t whitein_period;
    // Whether completion marks the stage cleared and hits the player.
    bool clears_stage;
} BOSS_DEFEAT_PARAMS;

// One boss's lifecycle, with its own assets that gameplay never reads.
// Optional operations are nullptr.
typedef struct {
    size_t assets_size;
    RESULT (*load_assets)(VFS *vfs, const ALLOCATOR *allocator, void *out);
    // Resets the boss state, before the first fight of a game.
    void (*init)(void);
    // Selects g_Boss and binds the assets.
    void (*setup)(const void *assets, const BOSS_CONTEXT *ctx);
    bool (*resume_setup)(const void *assets, const BOSS_CONTEXT *ctx);
    void (*begin_entrance)(const void *assets);
    bool (*resume_entrance)(const void *assets, const BOSS_CONTEXT *ctx);
    // Runs one battle call; false suspends the owning world.
    bool (*update)(const void *assets, const BOSS_CONTEXT *ctx);
    // The stage background, or nullptr for the scene's.
    const PI_IMAGE *(*background)(const void *assets);
    // Runs when the stage objects have entered.
    void (*finish_entrance)(void);
    // Runs at the start of each life.
    void (*begin_life)(void);
    // Whether the first battle call runs at the start of each life.
    bool update_on_life_begin;
    // The battle music, or -1 for none started by gameplay.
    int8_t song;
    // How this boss's defeat sequence plays out.
    BOSS_DEFEAT_PARAMS defeat;
} BOSS_STRATEGY;

// Returns the lifecycle of a boss other than BOSS_NONE.
const BOSS_STRATEGY *BossStrategy_Get(BOSS boss);

// Returns the boss fought on the given stage and route, or BOSS_NONE for a
// stage with no boss.
BOSS BossStrategy_ForStage(int16_t stage, int16_t route);
