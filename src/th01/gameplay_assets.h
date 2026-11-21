#pragma once

#include "th01/bonus.h"
#include "th01/boss_b05.h"
#include "th01/boss_b10j.h"
#include "th01/boss_b10m.h"
#include "th01/boss_b15j.h"
#include "th01/boss_b15m.h"
#include "th01/boss_b20j.h"
#include "th01/boss_b20m.h"
#include "pc98/mdrv_synth.h"
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
    B05_ASSETS b05;
    B10J_ASSETS b10j;
    B10M_ASSETS b10m;
    B15J_ASSETS b15j;
    B15M_ASSETS b15m;
    B20J_ASSETS b20j;
    B20M_ASSETS b20m;
    BONUS_TOTAL_ASSETS total;
    BONUS_STAGE_ASSETS *bonus;
    PELLET_ASSETS *pellets;
    SHOOTOUT_LASERS_ASSETS *lasers;
    GAME_TEXT_FONT hud_font, item_font, bonus_font, route_font, menu_font;
    MDRV_MUSIC songs[GAMEPLAY_SONG_COUNT];
    MDRV_EFFECTS effects;
} GAMEPLAY_ASSETS;

// Loads and validates every gameplay scene and boss before simulation. The
// VFS must contain the mounted original archive and REIIDEN.EXE. Retain the
// allocator group with the assets; failures leave intermediate allocations
// in that group. No original data is written. Font glyphs come from
// rom. Loads all music for a prepared deterministic audio producer;
// audio device delivery belongs to the application.
RESULT GameplayAssets_Load(
    VFS *vfs, const FONT_ROM *rom, const ALLOCATOR *allocator,
    GAMEPLAY_ASSETS *out);
