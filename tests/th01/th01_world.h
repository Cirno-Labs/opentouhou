#pragma once

#include "th01/bomb.h"
#include "th01/bonus.h"
#include "th01/boss.h"
#include "th01/extend.h"
#include "th01/gameplay.h"
#include "th01/hud.h"
#include "th01/hud/hp.h"
#include "th01/items.h"
#include "th01/miss.h"
#include "th01/missiles.h"
#include "th01/orb.h"
#include "th01/palette.h"
#include "th01/particles.h"
#include "th01/pellets.h"
#include "th01/player.h"
#include "th01/rng.h"
#include "th01/scene.h"
#include "th01/session.h"
#include "th01/shootout_lasers.h"
#include "th01/shots.h"
#include "th01/stage.h"
#include "th01/stage/objects.h"

#include <string.h>

// Zeroes the gameplay module state as a world reset does.
static inline void Th01Test_ResetWorld(void)
{
    Player_Init();
    Orb_Init();
    Shots_Init();
    Pellets_Init();
    Palette_Init(&g_Palette);
    memset(&g_StagePalette, 0, sizeof(g_StagePalette));
    Boss_Init();
    g_Rng = 0;
    StageObjects_Clear();
    Stage_Init();
    StageOrb_Init();
    Items_Init();
    Missiles_Init();
    ShootoutLasers_Init();
    Particles_Reset();
    Bomb_Init();
    Miss_Init();
    Scene_Init();
    Bonus_Init();
    Extend_Reset();
    Extend_SyncResidentLives(0);
    g_Session.lives = 0;
    g_Session.high_score = 0;
    g_Session.point_value = 0;
    g_Session.stage_id = 0;
    Hud_Reset();
    HudHp_Reset();
}
