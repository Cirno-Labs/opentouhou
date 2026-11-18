#pragma once

#include "script.h"
#include "session_compare.h"
#include "session.h"
#include "session_state.h"
#include "shell/game/game.h"

typedef struct {
    const GAME_DESC *game;
    const SCRIPT_BINDING *bindings;
    size_t binding_count;
    const char *replay_copy;
    void (*set_buttons)(void *input, uint16_t word);
    // Replaces only the displayed counter, leaving rate measurements alone.
    void (*set_fps)(const SCRIPT_FPS *observation);
    // Sets the same initial RNG and sound settings as the retail shim.
    void (*prepare)(void);
    const RUNNER_GAME *comparison;
    bool (*case_ready)(void);
    bool (*case_ended)(void);
    void (*point)(SESSION_POINT *point);
    const SESSION_ACTION *actions;
    size_t action_count;
} SESSION_GAME;

// Runs the real application callbacks with scripted input and one
// presentation per tick. --out must name a new directory; all saves go
// into its saves/ directory. The screenshot controller supplies stdin.
int Session_Main(const SESSION_GAME *game, int argc, char **argv);

extern const SESSION_GAME g_SessionGame;
