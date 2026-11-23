#pragma once

#include "th01/gameplay.h"

#include "th01/invincibility.h"
#include "th01/items.h"
#include "th01/miss.h"
#include "th01/sound.h"

// Keeps the complete world private to the gameplay implementation files.
typedef enum {
    GP_STAGE_BEGIN,
    GP_B10J_SETUP,
    GP_FINAL_ENTRANCE,
    GP_OBJECT_ENTRANCE,
    GP_TITLE,
    GP_STAGE_LABEL,
    GP_STAGE_NUMBER,
    GP_LIFE_BEGIN,
    GP_LIFE_BOSS,
    GP_WAIT_SHOT,
    GP_WAIT_SHOT_DELAY,
    GP_FRAME_BEGIN,
    GP_PLAYER_BOMB,
    GP_ITEMS,
    GP_TIMER,
    GP_BOSS,
    GP_ORB,
    GP_AFTER_ORB,
    GP_PAUSE,
    GP_PAUSE_DELAY,
    GP_QUIT_CONFIRM,
    GP_LIFE_EXIT,
    GP_MISS,
    GP_STAGE_BONUS,
    GP_TOTAL_BONUS,
    GP_GAMEOVER,
    GP_GAMEOVER_DELAY,
    GP_GAMEOVER_END,
    GP_CONTINUE,
    GP_CONTINUE_DELAY,
    GP_VICTORY_DELAY,
    GP_SWITCH,
    GP_TERMINAL,
} GAMEPLAY_PHASE;

// Names the boss of the stage, or none. Gameplay_HookBossHp indexes by it.
typedef enum {
    BOSS_NONE,
    BOSS_SINGYOKU,
    BOSS_YUUGENMAGAN,
    BOSS_MIMA,
    BOSS_KIKURI,
    BOSS_ELIS,
    BOSS_SARIEL,
    BOSS_KONNGARA,
} BOSS;

// Names the program that game_switch_binary's black-out leads to: REIIDEN
// again for a boss stage or a Continue, OP or FUUIN.
typedef enum {
    GAMEPLAY_SWITCH_STAGE,
    GAMEPLAY_SWITCH_CONTINUE,
    GAMEPLAY_SWITCH_OPENING,
    GAMEPLAY_SWITCH_ENDING,
} GAMEPLAY_SWITCH;

typedef struct {
    GAMEPLAY_REQUEST request;
    GAMEPLAY_PHASE phase, after_player;
    FRAME_WAIT wait;
    uint32_t frame, score_bonus;
    bool first_stage, wait_for_shot, timer_initialized, game_cleared;
    bool test_damage, boss_stage, waiting, bgm_started;
    BOSS boss;
    uint8_t menu_selection, animation_frame;
    int16_t gameover_left[5], gameover_top[5];
    bool gameover_clipped[5];
    uint16_t continue_frame;
    // Ignores confirm at the Continue prompt until it is released, with
    // fix_debouncing.
    bool continue_locked;
    GAMEPLAY_SWITCH switch_to;
    PALETTE_FADE switch_fade;
    PLAYER_BOMB_CONTINUATION player_bomb;
    INVINCIBILITY invincibility;

} GAMEPLAY_WORLD;

typedef struct {
    const GAMEPLAY_ASSETS *assets;
    GAME_AUDIO *audio_output;
    uint64_t host_tick;
    const GAMEPLAY_WORLD *initial;
} GAMEPLAY_STORAGE;

typedef struct {
    PLAYER_SPRITES player_sprites;
    PLAYER_OUTPUT player_output;
    SHOTS_OUTPUT shots_output;
    STAGE_CONTEXT stage;
    ITEM_CONTEXT item;
    PELLET_CONTEXT pellet;
    BOMB_CONTEXT bombing;
    MISS_CONTEXT missing;
    ROUTE_SELECT_ASSETS route_assets;
    ROUTE_SELECT_CONTEXT route;
    BOSS_HIT_CONTEXT hit;
    B05_CONTEXT c05;
    B10J_CONTEXT c10j;
    B10M_CONTEXT c10m;
    B15J_CONTEXT c15j;
    B15M_CONTEXT c15m;
    B20J_CONTEXT c20j;
    B20M_CONTEXT c20m;
    BONUS_CONTEXT bonus_context;
    BONUS_TOTAL_CONTEXT total_context;
} GAMEPLAY_BINDINGS;

struct GAMEPLAY {
    GAMEPLAY_STORAGE storage;
    GAMEPLAY_WORLD world;
    GAMEPLAY_BINDINGS bindings;
};

void Gameplay_Bind(GAMEPLAY *gameplay);
void Gameplay_ResetWorld(GAMEPLAY *gameplay);
void Gameplay_ResetInput(GAMEPLAY *gameplay);
void Gameplay_DefaultActors(GAMEPLAY *gameplay);
bool Gameplay_BossTick(GAMEPLAY *gameplay, const TH01_INPUT *raw);
void Gameplay_BossSetup(GAMEPLAY *gameplay);
void Gameplay_HookBossHp(GAMEPLAY *gameplay);
void Gameplay_SyncSession(GAMEPLAY *gameplay);
