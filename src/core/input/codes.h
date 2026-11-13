// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <stdint.h>

// Keyboard codes are USB HID usage ids, which all lie below 0x100. The
// modifier keys go in HID order from KEY_LEFT_MODIFIERS (left_ctrl,
// left_shift, left_alt, left_gui) and from KEY_RIGHT_MODIFIERS. The generic
// modifiers ctrl, shift, alt and gui take the codes from KEY_GENERIC_MODIFIERS
// up, in the same order, outside the HID range so that no key can have
// them. A generic modifier is held while its left or right key is held.
#define KEY_LEFT_MODIFIERS 0xE0
#define KEY_RIGHT_MODIFIERS 0xE4
#define KEY_GENERIC_MODIFIERS 0x100
#define KEY_MODIFIER_COUNT 4

// Gamepad codes: positions in the standard gamepad layout. GAMEPAD_SOUTH
// through GAMEPAD_DPAD_RIGHT match the same-numbered bit positions of
// INPUT_CONTROLLER_STATE.buttons; the sticks
// match INPUT_CONTROLLER_STATE.axes as axis 0-1 (left stick x, y) and axis
// 2-3 (right stick x, y); the triggers match axis 4 and axis 5, each from
// 0 to 1.
typedef enum {
    GAMEPAD_SOUTH,
    GAMEPAD_EAST,
    GAMEPAD_WEST,
    GAMEPAD_NORTH,
    GAMEPAD_LEFT_SHOULDER,
    GAMEPAD_RIGHT_SHOULDER,
    GAMEPAD_BACK,
    GAMEPAD_START,
    GAMEPAD_GUIDE,
    GAMEPAD_LEFT_STICK_PRESS,
    GAMEPAD_RIGHT_STICK_PRESS,
    GAMEPAD_DPAD_UP,
    GAMEPAD_DPAD_DOWN,
    GAMEPAD_DPAD_LEFT,
    GAMEPAD_DPAD_RIGHT,
    GAMEPAD_LEFT_TRIGGER,
    GAMEPAD_RIGHT_TRIGGER,
    GAMEPAD_LEFT_STICK_UP,
    GAMEPAD_LEFT_STICK_DOWN,
    GAMEPAD_LEFT_STICK_LEFT,
    GAMEPAD_LEFT_STICK_RIGHT,
    GAMEPAD_RIGHT_STICK_UP,
    GAMEPAD_RIGHT_STICK_DOWN,
    GAMEPAD_RIGHT_STICK_LEFT,
    GAMEPAD_RIGHT_STICK_RIGHT,
    GAMEPAD_LEFT_STICK,
    GAMEPAD_RIGHT_STICK,
} GAMEPAD_CODE;

#define GAMEPAD_AXIS_LEFT_STICK_X 0
#define GAMEPAD_AXIS_LEFT_STICK_Y 1
#define GAMEPAD_AXIS_RIGHT_STICK_X 2
#define GAMEPAD_AXIS_RIGHT_STICK_Y 3
#define GAMEPAD_AXIS_LEFT_TRIGGER 4
#define GAMEPAD_AXIS_RIGHT_TRIGGER 5

// Joystick codes pack a tag and an index into a uint16_t. JOY_BUTTON holds
// the button index directly. JOY_HAT_DIR holds hat*4 + direction (0 up, 1
// down, 2 left, 3 right), matching the bit order of
// INPUT_CONTROLLER_STATE.hats. JOY_AXIS_DIR holds axis*2 + direction (0
// negative, 1 positive), for using an axis as a button. JOY_AXIS holds the
// axis index, for the whole axis in a stick action.
#define JOY_TAG_BUTTON 0x0000
#define JOY_TAG_HAT_DIR 0x1000
#define JOY_TAG_AXIS_DIR 0x2000
#define JOY_TAG_AXIS 0x3000
#define JOY_TAG_MASK 0xF000
#define JOY_INDEX_MASK 0x0FFF

#define JOY_HAT_DIR_UP 0
#define JOY_HAT_DIR_DOWN 1
#define JOY_HAT_DIR_LEFT 2
#define JOY_HAT_DIR_RIGHT 3

#define JOY_AXIS_DIR_NEGATIVE 0
#define JOY_AXIS_DIR_POSITIVE 1

static inline uint16_t Joy_Button(const int index)
{
    return (uint16_t)(JOY_TAG_BUTTON | index);
}

static inline uint16_t Joy_HatDir(const int hat, const int dir)
{
    return (uint16_t)(JOY_TAG_HAT_DIR | (hat * 4 + dir));
}

static inline uint16_t Joy_AxisDir(const int axis, const int dir)
{
    return (uint16_t)(JOY_TAG_AXIS_DIR | (axis * 2 + dir));
}

static inline uint16_t Joy_Axis(const int axis)
{
    return (uint16_t)(JOY_TAG_AXIS | axis);
}
