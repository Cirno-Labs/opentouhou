#pragma once

#include "core/app.h"

#include <stddef.h>
#include <stdint.h>

// Stands in for core/app in the th06 tests: the functions that th06 calls
// read g_FakeApp and record their requests in it. FakeApp_Reset gives the
// state of the replay runner's core/app: no pad press, no bindings, no
// requests and the windowed mode.

#define FAKE_APP_REQUEST_COUNT 32
#define FAKE_APP_BINDING_COUNT 4

// Holds one request. kind is 'b' for a bind, 'u' for an unbind and 'r' for
// the pad reset.
typedef struct {
    char kind;
    size_t action;
    INPUT_BINDING binding;
} FAKE_APP_REQUEST;

typedef struct {
    // Holds the control that the next App_GetPadPress returns once.
    bool pad_pending;
    INPUT_BINDING pad_press;
    // Holds the bindings that App_GetBindings returns for each action.
    INPUT_BINDING bindings[INPUT_MAX_ACTIONS][FAKE_APP_BINDING_COUNT];
    size_t binding_counts[INPUT_MAX_ACTIONS];
    FAKE_APP_REQUEST requests[FAKE_APP_REQUEST_COUNT];
    int32_t request_count;
} FAKE_APP;

extern FAKE_APP g_FakeApp;

void FakeApp_Reset(void);
