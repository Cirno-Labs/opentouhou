#include "shell/game/request_queue.h"

#include "core/base/log.h"
#include "core/input/codes.h"

#include <string.h>

#define M_MAX_BINDINGS 32

static bool M_IsModifierOrLock(const INPUT_BINDING binding)
{
    const uint16_t trigger = binding.codes[binding.count - 1];
    return (trigger >= KEY_LEFT_MODIFIERS
            && trigger < KEY_RIGHT_MODIFIERS + KEY_MODIFIER_COUNT)
        || trigger == 0x39 || trigger == 0x47 || trigger == 0x53;
}

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

static bool M_IsBound(
    const INPUT_STATE *const input, const INPUT_SCHEMA *const schema,
    const INPUT_BINDING binding)
{
    for (size_t action = 0; action < schema->count; action++) {
        INPUT_BINDING bindings[M_MAX_BINDINGS];
        const size_t count = Input_GetBindings(
            input, schema, 0, action, bindings, M_MAX_BINDINGS);
        for (size_t i = 0; i < count && i < M_MAX_BINDINGS; i++) {
            if (bindings[i].device == binding.device
                && bindings[i].count == binding.count
                && memcmp(
                       bindings[i].codes, binding.codes,
                       binding.count * sizeof(binding.codes[0]))
                    == 0) {
                return true;
            }
        }
    }
    return false;
}

void ShellRequests_PollPresses(
    SHELL_INPUT_PRESSES *const presses, INPUT_STATE *const input,
    const INPUT_SCHEMA *const skipped)
{
    INPUT_BINDING key;
    if (Input_LastPressed(input, INPUT_DEVICE_KEYBOARD, &key)
        && !M_IsBound(input, skipped, key)
        && (!presses->key_pending
            || (M_IsModifierOrLock(presses->key)
                && !M_IsModifierOrLock(key)))) {
        presses->key_pending = true;
        presses->key = key;
    }
    INPUT_BINDING pad;
    if (ShellRequests_ReadPadPress(input, &pad) && !presses->pad_pending) {
        presses->pad_pending = true;
        presses->pad = pad;
    }
}

void ShellRequests_ClearPresses(SHELL_INPUT_PRESSES *const presses)
{
    presses->key_pending = false;
    presses->pad_pending = false;
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
