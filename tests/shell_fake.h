#pragma once

#include "shell/game.h"

#include <stddef.h>
#include <stdint.h>

// Stands in for shell/game in the game tests: the functions that a game calls
// read g_FakeShell and record their requests in it. FakeShell_Reset gives the
// state of the replay runner's shell/game before its first measurement: no
// pad press, no tick rate, no device input, no bindings, no requests and the
// windowed mode.

#define FAKE_SHELL_REQUEST_COUNT 32
#define FAKE_SHELL_BINDING_COUNT 4
#define FAKE_SHELL_DEVICE_INPUT_SIZE 64

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
    // Holds what Shell_GetDeviceInput writes for each device kind, and its
    // size; a size of 0 means unset.
    uint8_t device_input[INPUT_DEVICE_COUNT][FAKE_SHELL_DEVICE_INPUT_SIZE];
    size_t device_input_sizes[INPUT_DEVICE_COUNT];
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
// Sets what Shell_GetDeviceInput writes for device: size bytes of input,
// the game's input struct. Every device read must be set first.
void FakeShell_SetDeviceInput(
    INPUT_DEVICE device, const void *input, size_t size);
