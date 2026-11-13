#include "request_queue.h"

#include "core/base/log.h"

#define M_MAX_BINDINGS 32

static void M_ResetPad(
    INPUT_STATE *const input, const INPUT_SCHEMA *const schema)
{
    INPUT_BINDING keyboard[INPUT_MAX_ACTIONS][M_MAX_BINDINGS];
    size_t counts[INPUT_MAX_ACTIONS] = {};
    for (size_t action = 0; action < schema->count; action++) {
        INPUT_BINDING bindings[M_MAX_BINDINGS];
        const size_t count = Input_GetBindings(
            input, schema, 0, action, bindings, M_MAX_BINDINGS);
        for (size_t i = 0; i < count && i < M_MAX_BINDINGS; i++) {
            if (bindings[i].device == INPUT_DEVICE_KEYBOARD) {
                keyboard[action][counts[action]++] = bindings[i];
            }
        }
    }
    Input_ResetBindings(input, schema, 0);
    for (size_t action = 0; action < schema->count; action++) {
        INPUT_BINDING bindings[M_MAX_BINDINGS];
        const size_t count = Input_GetBindings(
            input, schema, 0, action, bindings, M_MAX_BINDINGS);
        for (size_t i = 0; i < count && i < M_MAX_BINDINGS; i++) {
            if (bindings[i].device == INPUT_DEVICE_KEYBOARD) {
                Input_Unbind(input, schema, 0, action, bindings[i]);
            }
        }
        for (size_t i = 0; i < counts[action]; i++) {
            Input_Bind(input, schema, 0, action, keyboard[action][i]);
        }
    }
}

bool ShellRequests_ReadPadPress(
    INPUT_STATE *const input, INPUT_BINDING *const out)
{
    INPUT_BINDING gamepad;
    INPUT_BINDING joystick;
    const bool from_gamepad =
        Input_LastPressed(input, INPUT_DEVICE_GAMEPAD, &gamepad);
    const bool from_joystick =
        Input_LastPressed(input, INPUT_DEVICE_JOYSTICK, &joystick);
    if (!from_gamepad && !from_joystick) {
        return false;
    }
    const INPUT_BINDING *const chord = from_gamepad ? &gamepad : &joystick;
    *out = (INPUT_BINDING) {
        .device = chord->device,
        .count = 1,
        .codes = { chord->codes[chord->count - 1] },
    };
    return true;
}

void ShellRequests_Add(
    SHELL_REQUESTS *const requests, const SHELL_REQUEST request)
{
    if (requests->count >= SHELL_REQUEST_COUNT) {
        LOG_WARNING("Dropped a request of the game to shell/game");
        return;
    }
    requests->items[requests->count++] = request;
}

void ShellRequests_ApplyInput(
    SHELL_REQUESTS *const requests, INPUT_STATE *const input,
    const INPUT_SCHEMA *const schema)
{
    for (int32_t i = 0; i < requests->count; i++) {
        const SHELL_REQUEST *const request = &requests->items[i];
        switch (request->type) {
        case SHELL_REQUEST_BIND:
            Input_Bind(input, schema, 0, request->action, request->binding);
            break;
        case SHELL_REQUEST_UNBIND:
            Input_Unbind(input, schema, 0, request->action, request->binding);
            break;
        case SHELL_REQUEST_RESET_PAD:
            M_ResetPad(input, schema);
            break;
        }
    }
    requests->count = 0;
}
