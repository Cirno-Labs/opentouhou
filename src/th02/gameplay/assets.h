#pragma once

#include "th02/gaiji.h"
#include "th02/bullets.h"
#include "th02/patterns.h"
#include "th02/point_numbers.h"
#include "th02/sparks.h"
#include "th02/stage/backdrop.h"
#include "th02/stage_bonus/assets.h"
#include "th02/stage/titles.h"
#include "th02/boss/fight.h"
#include "th02/pause_menu.h"

#include "core/vfs.h"

typedef struct {
    PATTERN_BANK patterns;
    GAIJI gaiji;
    POINT_NUMBER_FONT numbers;
    SPARK_IMAGES sparks;
    GFX_COLOR sprite_palette[16];
    bool has_sprite_palette;
    BULLET_IMAGES bullets;
    STAGE_BACKDROP backdrop;
    STAGE_BONUS_ASSETS bonus;
    STAGE_TITLE_ASSETS titles;
    BOSS_ASSETS bosses;
    PAUSE_MENU_ASSETS pause;
} GAMEPLAY_ASSETS;

// Loads the gameplay sprites; the allocator must free individually.
RESULT GameplayAssets_Load(
    VFS *files, const ALLOCATOR *allocator, GAMEPLAY_ASSETS *out);

// Frees the sprites once nothing borrows them; accepts zeroed storage.
void GameplayAssets_Free(GAMEPLAY_ASSETS *assets);
