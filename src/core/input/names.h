#pragma once

#include "core/input/schema.h"

// Writes the name of a binding into out, such as "left_shift", "south",
// "alt+return" or "back+start". The parts of a chord are joined with "+".
// An unknown control gives an empty name.
void Input_BindingName(
    INPUT_BINDING binding, char out[INPUT_BINDING_NAME_SIZE]);

// Finds the binding with a name, chord or not. Spaces around each part are
// ignored. Returns false when any part is unknown, when the parts belong to
// different device kinds, or when there are more than INPUT_CHORD_MAX
// parts.
bool Input_BindingFromName(const char *name, INPUT_BINDING *out);
