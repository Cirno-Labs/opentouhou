#pragma once

#include "th01/gameplay.h"

#include "th01/invincibility.h"
#include "th01/items.h"
#include "th01/miss.h"

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
    GP_TERMINAL,
} GAMEPLAY_PHASE;

struct GAMEPLAY {
    const GAMEPLAY_ASSETS *assets;
    VIDEO *video;
    SESSION *session;
    MDRV_SYNTH *audio;
    size_t song;
    GAMEPLAY_REQUEST request;
    GAMEPLAY_PHASE phase, after_player;
    FRAME_VSYNC vsync;
    FRAME_WAIT wait;
    uint64_t host_tick;
    uint32_t rng, frame, score_bonus;
    int16_t next_extend;
    int8_t retained_lives;
    bool first_stage, wait_for_shot, timer_initialized, game_cleared;
    bool test_damage, boss_stage, waiting, bgm_started;
    uint8_t boss, menu_selection, animation_frame;
    int16_t gameover_left[5], gameover_top[5];
    bool gameover_clipped[5];
    uint16_t continue_frame;
    STAGE_GAMEPLAY metrics;
    STAGE_OBJECTS objects;
    STAGE_OBJECT_STORAGE storage;
    STAGE_ORB_COMPOSITE composite;
    PLAYER player;
    GAME_INPUT input;
    PLAYER_BOMB_CONTINUATION player_bomb;
    ORB orb;
    SHOTS shots;
    PELLETS pellets;
    PELLET_SHARED pellet_shared;
    ITEMS items;
    MISSILES missiles;
    SHOOTOUT_LASERS lasers;
    PARTICLES particles;
    INVINCIBILITY invincibility;
    BOMB bomb;
    MISS miss;
    HUD hud;
    HUD_HP health;
    TRAM tram;
    STAGE_TIMER timer;
    PALETTE_STATE palette;
    PALETTE stage_palette;
    SCENE_STATE scene;
    BOSS_SPRITES sprites;
    B05 b05;
    B10J b10j;
    B10M b10m;
    B15J b15j;
    B15M b15m;
    B20J b20j;
    B20M b20m;
    BONUS bonus;
    BONUS_TOTAL total;
    PLAYER_SPRITES player_sprites;
    PLAYER_OUTPUT player_output;
    SHOTS_OUTPUT shots_output;
    HUD_CONTEXT display;
    EXTEND_CONTEXT extend;
    STAGE_CONTEXT stage;
    ITEM_CONTEXT item;
    PELLET_CONTEXT pellet;
    BOMB_CONTEXT bombing;
    MISS_CONTEXT missing;
    TIMER_CONTEXT timing;
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
};

void Gameplay_Bind(GAMEPLAY *gameplay);
void Gameplay_ResetWorld(GAMEPLAY *gameplay);
void Gameplay_Effect(void *user, int16_t number);
void Gameplay_Play(GAMEPLAY *gameplay, size_t song);
void Gameplay_ResetInput(GAMEPLAY *gameplay);
void Gameplay_DefaultActors(GAMEPLAY *gameplay);
bool Gameplay_BossTick(GAMEPLAY *gameplay, const TH01_INPUT *raw);
void Gameplay_BossSetup(GAMEPLAY *gameplay);
void Gameplay_SyncSession(GAMEPLAY *gameplay);
