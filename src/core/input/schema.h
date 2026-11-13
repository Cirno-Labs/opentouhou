// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "core/base/utils.h"

#include <stddef.h>
#include <stdint.h>

// Reads the keyboard, gamepads and joysticks into the game's own input
// struct. The game declares the struct, with a bool per
// button and two floats per stick, and a table of INPUT_ACTIONs that maps
// each field to its default bindings. The INI key of an action is its field
// name.
//
// Example:
// typedef struct {
//     bool shoot;
//     bool left;
//     float move_x;
//     float move_y;
// } TH06_INPUT;
//
// static const INPUT_ACTION m_Actions[] = {
//     INPUT_BUTTON(TH06_INPUT, shoot, "z", "south", "joy_button_0"),
//     INPUT_BUTTON(TH06_INPUT, left, "left, keypad_4",
//         "dpad_left, left_stick_left", "joy_hat_left"),
//     INPUT_STICK(TH06_INPUT, move, "left_stick", "joy_axis_0, joy_axis_1"),
// };
// static const INPUT_SCHEMA m_Schema =
//     INPUT_SCHEMA_OF(TH06_INPUT, m_Actions, 1);
//
// TH06_INPUT input;
// Input_Update(state, &input);
// if (input.shoot) { ... }

#define INPUT_MAX_PLAYERS 2
#define INPUT_MAX_ACTIONS 32
#define INPUT_MAX_SCHEMAS 4
#define INPUT_CHORD_MAX 4
#define INPUT_BINDING_NAME_SIZE 64

typedef enum {
    INPUT_DEVICE_KEYBOARD,
    // A controller that the platform recognizes, with buttons named by
    // position.
    INPUT_DEVICE_GAMEPAD,
    // A controller that the platform does not recognize, with numbered
    // buttons, axes and hats.
    INPUT_DEVICE_JOYSTICK,
    INPUT_DEVICE_COUNT,
} INPUT_DEVICE;

typedef enum {
    INPUT_ACTION_BUTTON,
    INPUT_ACTION_STICK,
} INPUT_ACTION_TYPE;

// Identifies a chord of one to INPUT_CHORD_MAX controls of one device kind.
// The last control is the trigger; the others are modifiers. A control is a
// key, button, trigger, hat direction, stick direction or stick. The codes
// are private to core/input; Input_BindingName gives the name.
//
// A one-control binding is held while its control is held. A chord becomes
// held when its trigger goes down while every modifier is held, also when
// they go down in the same update, and stays held until any of its controls
// is released. A trigger that goes down before a modifier does not start
// the chord. While a chord is held, every other binding of every schema
// whose controls are all in the chord is not held. The keyboard modifiers ctrl,
// shift, alt and gui are held while their left or right key is held, and
// match both keys in a chord. A whole stick cannot be part of a chord.
typedef struct {
    INPUT_DEVICE device;
    uint8_t count;
    uint16_t codes[INPUT_CHORD_MAX];
} INPUT_BINDING;

// Describes one action. INPUT_BUTTON and INPUT_STICK fill it in.
typedef struct {
    INPUT_ACTION_TYPE type;
    // Sets the INI key, which is the field name. For a stick, the name
    // without the _x and _y suffixes.
    const char *name;
    // Sets the offset of the bool, or of the stick's x float. The y float
    // follows the x float.
    size_t offset;
    // Sets the default bindings per device, in the INI value syntax, or
    // nullptr for none.
    const char *defaults[INPUT_DEVICE_COUNT];
} INPUT_ACTION;

typedef struct {
    const INPUT_ACTION *actions;
    size_t count;
    // Sets the size of the game's input struct.
    size_t size;
    // Sets how many players the game has, 1 or 2.
    int32_t players;
} INPUT_SCHEMA;

// The rest of this header, down to the macros, is the implementation behind
// INPUT_BUTTON, INPUT_STICK and INPUT_SCHEMA_OF. INPUT_FIELD_CHECK fails to
// compile when a field's type in type_ is not a pointer-compatible match for
// c_type_, which is how the macros below catch an action declared over the
// wrong kind of field.

#define INPUT_FIELD_CHECK(type_, field_, c_type_)                              \
    _Generic(&((type_ *)nullptr)->field_, c_type_ *: (size_t)0)

// Declares a button: a bool field that is true while any of its bindings is
// held. A stick direction bound to a button is held past the threshold.
#define INPUT_BUTTON(type_, field_, keyboard_, gamepad_, joystick_)            \
    {                                                                          \
        .type = INPUT_ACTION_BUTTON,                                           \
        .name = #field_,                                                       \
        .offset =                                                              \
            offsetof(type_, field_) + INPUT_FIELD_CHECK(type_, field_, bool),  \
        .defaults = { [INPUT_DEVICE_KEYBOARD] = (keyboard_),                   \
                      [INPUT_DEVICE_GAMEPAD] = (gamepad_),                     \
                      [INPUT_DEVICE_JOYSTICK] = (joystick_) },                 \
    }

// Declares a stick: the floats field_##_x and field_##_y, each from -1 to 1,
// with x positive to the right and y positive downward. Inside the
// deadzone, both are 0. Outside it, the vector is rescaled so that its
// length goes from 0 at the deadzone edge to 1 at full lean, and never
// exceeds 1. The values are quantized to steps of 1/32767, so a replay
// stores them exactly as int16. field_##_y must be declared directly after
// field_##_x in type_; a gap between them fails to compile.
#define INPUT_STICK(type_, field_, gamepad_, joystick_)                        \
    {                                                                          \
        .type = INPUT_ACTION_STICK,                                            \
        .name = #field_,                                                       \
        .offset = offsetof(type_, field_##_x)                                  \
            + INPUT_FIELD_CHECK(type_, field_##_x, float)                      \
            + INPUT_FIELD_CHECK(type_, field_##_y, float)                      \
            + sizeof(char                                                      \
                         [(offsetof(type_, field_##_y)                         \
                           == offsetof(type_, field_##_x) + sizeof(float))     \
                              ? 1                                              \
                              : -1])                                           \
                * 0,                                                           \
        .defaults = { [INPUT_DEVICE_GAMEPAD] = (gamepad_),                     \
                      [INPUT_DEVICE_JOYSTICK] = (joystick_) },                 \
    }

// Builds the INPUT_SCHEMA for an input struct type and its action table.
#define INPUT_SCHEMA_OF(type_, actions_, players_)                             \
    { (actions_), ARRAY_SIZE(actions_), sizeof(type_), (players_) }
