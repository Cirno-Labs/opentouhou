#pragma once

#include "th02/dialogue/assets.h"
#include "th02/boss/marisa/assets.h"

// Each landed boss adds its own bank by value here (as e.g. MARISA_ASSETS
// marisa;), once its asset type exists.
typedef struct {
    DIALOGUE_ASSETS stages[6];
    DIALOGUE_SCRIPTS scripts;
    MARISA_ASSETS marisa;
} BOSS_ASSETS;

typedef enum {
    BOSS_MUSIC_START,
    BOSS_MUSIC_STOP,
    BOSS_MUSIC_FADE,
} BOSS_MUSIC_REQUEST;

typedef struct {
    // Starts the stage's boss music.
    void (*boss)(void *context);
    // Drives a boss's own mid-fight music change (Mima's form transition).
    void (*transition)(void *context, BOSS_MUSIC_REQUEST request);
    void *context;
} BOSS_MUSIC;
