#pragma once

#include "th01/bomb.h"
#include "th01/bonus.h"
#include "th01/boss.h"
#include "th01/pellets.h"
#include "th01/scene.h"
#include "th01/shootout_lasers.h"
#include "pc98/mdrv.h"
#include "th01/invincibility.h"

#define GAMEPLAY_SONG_COUNT 18

typedef struct {
    SCENE_DATA scenes[SCENE_COUNT];
    PI_IMAGE backgrounds[SCENE_COUNT];
    PTN stage, boss_stage, miko;
    BOS miko_ac, miko_ac2;
    GRF hud;
    INVINCIBILITY_ASSETS invincibility;
    BOMB_ASSETS bomb;
    // Each boss strategy's own assets, indexed by BOSS.
    const void *bosses[BOSS_COUNT];
    BONUS_TOTAL_ASSETS total;
    BONUS_STAGE_ASSETS *bonus;
    PELLET_ASSETS *pellets;
    SHOOTOUT_LASERS_ASSETS *lasers;
    GAME_TEXT_FONT hud_font, item_font, bonus_font, route_font, menu_font;
    MDRV_MUSIC songs[GAMEPLAY_SONG_COUNT];
    MDRV_EFFECTS effects;
} GAMEPLAY_ASSETS;

// Loads and validates every gameplay scene, boss and music before simulation.
// Retain the allocator group with the assets, also on failure.
RESULT GameplayAssets_Load(
    VFS *vfs, const FONT_ROM *rom, const ALLOCATOR *allocator,
    GAMEPLAY_ASSETS *out);
