#pragma once

#include "th02/gameplay.h"
#include "th02/bomb/assets.h"
#include "th02/pause_menu.h"
#include "th02/wait.h"
#include "th02/hud.h"

typedef enum {
    STAGE_RUNNING,
    STAGE_INPUT,
    STAGE_PAUSE_INPUT,
    STAGE_PAUSE_FRAME,
    STAGE_FRAME_WAIT,
    STAGE_EXTERNAL,
    STAGE_QUIT,
    STAGE_NEXT,
} STAGE_EVENT;

typedef enum {
    GAMEPLAY_READY,
    GAMEPLAY_MUSIC_FADE,
    GAMEPLAY_PREPARE,
    GAMEPLAY_STAGE,
    GAMEPLAY_DONE,
    GAMEPLAY_REVEAL,
    GAMEPLAY_CONTINUE_COVER,
    GAMEPLAY_CONTINUE_REVEAL,
} GAMEPLAY_PHASE;

typedef enum {
    STAGE_TICK_BEGIN,
    STAGE_TICK_INPUT,
    STAGE_TICK_DEMO,
    STAGE_TICK_BOSS,
    STAGE_TICK_PAUSE,
    STAGE_TICK_FINISH,
    STAGE_TICK_WAIT,
    STAGE_TICK_ENTER,
    STAGE_TICK_EXIT,
} STAGE_TICK_PHASE;

typedef struct {
    STAGE_TICK_PHASE phase;
    STAGE_EVENT wait;
} STAGE_LOOP;

typedef struct {
    TH02_SESSION *session;
    const GAMEPLAY_ASSETS *source_assets;
    const STAGE_ASSETS *stage;
    const BOMB_ASSETS *bomb;
    const uint8_t *bomb_transition_tail;
    bool fix_bomb_transition_overread;
    GAMEPLAY_MUSIC music;
    GAMEPLAY_ASSETS assets;
    STAGE_LOOP loop;
    DEMO demo;
    bool demo_fading;
    WAIT wait;
    GAMEPLAY_PHASE phase;
    uint8_t bomb_tail[72];
    HUD_WIPE reveal;
    TH02_INPUT dialogue_input;
} GAMEPLAY;
