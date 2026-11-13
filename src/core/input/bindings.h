// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "core/input/schema.h"
#include "core/input/state.h"

#include <stddef.h>
#include <stdint.h>

// Writes up to capacity bindings of an action of schema into
// out and returns how many the action has.
size_t Input_GetBindings(
    const INPUT_STATE *input, const INPUT_SCHEMA *schema, int32_t player,
    size_t action, INPUT_BINDING *out, size_t capacity);

// Adds a binding to an action of schema. A binding already on
// the action does nothing, and so does a binding past the limit of 8
// bindings per device of an action.
void Input_Bind(
    INPUT_STATE *input, const INPUT_SCHEMA *schema, int32_t player,
    size_t action, INPUT_BINDING binding);

// Removes a binding from an action of schema. A binding not on
// the action does nothing.
void Input_Unbind(
    INPUT_STATE *input, const INPUT_SCHEMA *schema, int32_t player,
    size_t action, INPUT_BINDING binding);

// Sets every action of a player in schema back to the schema's defaults.
void Input_ResetBindings(
    INPUT_STATE *input, const INPUT_SCHEMA *schema, int32_t player);

// Returns whether a control of device went down since the previous call,
// and writes into out the chord that ends with the last one. For the
// keyboard, the chord starts with the held generic modifiers, in the order
// ctrl, shift, alt, gui. A modifier key that goes down is the trigger
// itself, after the other held modifiers. For a gamepad or a joystick, the
// chord starts with the other held controls, in ascending code order. A
// chord keeps at most INPUT_CHORD_MAX - 1 modifiers.
bool Input_LastPressed(
    INPUT_STATE *input, INPUT_DEVICE device, INPUT_BINDING *out);
