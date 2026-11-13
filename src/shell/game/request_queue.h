#pragma once

#include "core/input.h"

#include <stddef.h>
#include <stdint.h>

// Queues what a game asks of shell/game during a tick, so that shell/game
// changes its own state only after the tick.

#define APP_REQUEST_COUNT 64

typedef enum {
    APP_REQUEST_BIND,
    APP_REQUEST_UNBIND,
    APP_REQUEST_RESET_PAD,
} APP_REQUEST_TYPE;

typedef struct {
    APP_REQUEST_TYPE type;
    size_t action;
    INPUT_BINDING binding;
} APP_REQUEST;

typedef struct {
    APP_REQUEST items[APP_REQUEST_COUNT];
    int32_t count;
    // Holds the window mode asked for: -1 for none, 0 for windowed and 1
    // for fullscreen.
    int32_t fullscreen;
} APP_REQUESTS;

// Returns whether a gamepad control, or else a joystick control, went down
// since the previous call, and writes that one control into out.
bool AppRequests_ReadPadPress(INPUT_STATE *input, INPUT_BINDING *out);
// Queues request. A request past APP_REQUEST_COUNT in one tick is dropped
// with a warning.
void AppRequests_Add(APP_REQUESTS *requests, APP_REQUEST request);
// Applies the queued bindings to player 1 of schema, in order, and empties
// the queue. The pad reset sets the gamepad and joystick bindings of every
// action back to the defaults and keeps the keyboard bindings.
void AppRequests_ApplyInput(
    APP_REQUESTS *requests, INPUT_STATE *input, const INPUT_SCHEMA *schema);
