#pragma once

#include "th01/gameplay.h"

#include "th01/invincibility.h"
#include "th01/items.h"
#include "th01/miss.h"
#include "th01/sound.h"

// Names gameplay phases for the scene and replay state.
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
    uint32_t frame;
    bool wait_for_shot;
    bool boss_stage, waiting, bgm_started;
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

extern GAMEPLAY_WORLD g_Gameplay;
