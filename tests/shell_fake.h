#pragma once

#include "shell/game.h"

#include <stddef.h>
#include <stdint.h>

// Stands in for shell/game in the th06 tests: the functions that th06 calls
// read g_FakeShell and record their requests in it. FakeShell_Reset gives the
// state of the replay runner's shell/game before its first measurement: no
// pad press, no tick rate, no bindings, no requests and the windowed mode.

#define FAKE_SHELL_REQUEST_COUNT 32
#define FAKE_SHELL_BINDING_COUNT 4

// Holds one request. kind is 'b' for a bind, 'u' for an unbind, 'r' for the
// pad reset and 'f' for a window mode, with 1 for fullscreen in action.
typedef struct {
    char kind;
    size_t action;
    INPUT_BINDING binding;
} FAKE_SHELL_REQUEST;

typedef struct {
    // Holds the control that the next Shell_GetPadPress returns once.
    bool pad_pending;
    INPUT_BINDING pad_press;
    // Holds the tick rate that the next Shell_GetTickRate returns once.
    bool tick_rate_measured;
    float tick_rate;
    // Holds the bindings that Shell_GetBindings returns for each action.
    INPUT_BINDING bindings[INPUT_MAX_ACTIONS][FAKE_SHELL_BINDING_COUNT];
    size_t binding_counts[INPUT_MAX_ACTIONS];
    // Holds the window mode that Shell_IsFullscreen returns.
    bool fullscreen;
    FAKE_SHELL_REQUEST requests[FAKE_SHELL_REQUEST_COUNT];
    int32_t request_count;
} FAKE_SHELL;

extern FAKE_SHELL g_FakeShell;

void FakeShell_Reset(void);
