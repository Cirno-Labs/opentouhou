#pragma once

#include "script.h"
#include "session_compare.h"
#include "shell/game/game.h"

typedef struct {
    const GAME_DESC *game;
    const SCRIPT_BINDING *bindings;
    size_t binding_count;
    const char *replay_copy;
    void (*set_buttons)(void *input, uint16_t word);
    // Sets the same initial RNG and sound settings as the retail shim.
    void (*prepare)(void);
    const RUNNER_GAME *comparison;
    bool (*case_ready)(void);
    bool (*case_ended)(void);
} SESSION_GAME;

// Runs the real application callbacks with scripted input and one
// presentation per tick. --out must name a new directory; all saves go
// into its saves/ directory. The screenshot controller supplies stdin.
int Session_NativeMain(const SESSION_GAME *game, int argc, char **argv);
