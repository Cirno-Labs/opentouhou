#pragma once

#include "core/input.h"

#include <stddef.h>
#include <stdint.h>

// Queues what a game asks of shell/game during a tick, so that shell/game
// changes its own state only after the tick.

#define SHELL_REQUEST_COUNT 64

typedef enum {
    SHELL_REQUEST_BIND,
    SHELL_REQUEST_UNBIND,
    SHELL_REQUEST_RESET_PAD,
} SHELL_REQUEST_TYPE;

typedef struct {
    SHELL_REQUEST_TYPE type;
    size_t action;
    INPUT_BINDING binding;
} SHELL_REQUEST;

typedef struct {
    SHELL_REQUEST items[SHELL_REQUEST_COUNT];
    int32_t count;
    // Holds the window mode asked for: -1 for none, 0 for windowed and 1
    // for fullscreen.
    int32_t fullscreen;
} SHELL_REQUESTS;

// Returns whether a gamepad control, or else a joystick control, went down
// since the previous call, and writes that one control into out.
bool ShellRequests_ReadPadPress(INPUT_STATE *input, INPUT_BINDING *out);
// Queues request. A request past SHELL_REQUEST_COUNT in one tick is dropped
// with a warning.
void ShellRequests_Add(SHELL_REQUESTS *requests, SHELL_REQUEST request);
// Applies the queued bindings to player 1 of schema, in order, and empties
// the queue. The pad reset sets the gamepad and joystick bindings of every
// action back to the defaults and keeps the keyboard bindings.
void ShellRequests_ApplyInput(
    SHELL_REQUESTS *requests, INPUT_STATE *input, const INPUT_SCHEMA *schema);
