// SPDX-License-Identifier: GPL-3.0-only

#include "core/input.h"

#include "core/base/string.h"
#include "core/input/codes.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *name;
    uint16_t code;
} NAMED_CODE;

// HID usage ids, usage page 0x07 (keyboard/keypad).
static const NAMED_CODE m_Keyboard[] = {
    { "a", 0x04 },
    { "b", 0x05 },
    { "c", 0x06 },
    { "d", 0x07 },
    { "e", 0x08 },
    { "f", 0x09 },
    { "g", 0x0A },
    { "h", 0x0B },
    { "i", 0x0C },
    { "j", 0x0D },
    { "k", 0x0E },
    { "l", 0x0F },
    { "m", 0x10 },
    { "n", 0x11 },
    { "o", 0x12 },
    { "p", 0x13 },
    { "q", 0x14 },
    { "r", 0x15 },
    { "s", 0x16 },
    { "t", 0x17 },
    { "u", 0x18 },
    { "v", 0x19 },
    { "w", 0x1A },
    { "x", 0x1B },
    { "y", 0x1C },
    { "z", 0x1D },
    { "1", 0x1E },
    { "2", 0x1F },
    { "3", 0x20 },
    { "4", 0x21 },
    { "5", 0x22 },
    { "6", 0x23 },
    { "7", 0x24 },
    { "8", 0x25 },
    { "9", 0x26 },
    { "0", 0x27 },
    { "return", 0x28 },
    { "escape", 0x29 },
    { "backspace", 0x2A },
    { "tab", 0x2B },
    { "space", 0x2C },
    { "f1", 0x3A },
    { "f2", 0x3B },
    { "f3", 0x3C },
    { "f4", 0x3D },
    { "f5", 0x3E },
    { "f6", 0x3F },
    { "f7", 0x40 },
    { "f8", 0x41 },
    { "f9", 0x42 },
    { "f10", 0x43 },
    { "f11", 0x44 },
    { "f12", 0x45 },
    { "print_screen", 0x46 },
    { "insert", 0x49 },
    { "home", 0x4A },
    { "page_up", 0x4B },
    { "delete", 0x4C },
    { "end", 0x4D },
    { "page_down", 0x4E },
    { "right", 0x4F },
    { "left", 0x50 },
    { "down", 0x51 },
    { "up", 0x52 },
    { "keypad_slash", 0x54 },
    { "keypad_asterisk", 0x55 },
    { "keypad_minus", 0x56 },
    { "keypad_plus", 0x57 },
    { "keypad_enter", 0x58 },
    { "keypad_1", 0x59 },
    { "keypad_2", 0x5A },
    { "keypad_3", 0x5B },
    { "keypad_4", 0x5C },
    { "keypad_5", 0x5D },
    { "keypad_6", 0x5E },
    { "keypad_7", 0x5F },
    { "keypad_8", 0x60 },
    { "keypad_9", 0x61 },
    { "keypad_0", 0x62 },
    { "keypad_period", 0x63 },
    { "left_ctrl", 0xE0 },
    { "left_shift", 0xE1 },
    { "left_alt", 0xE2 },
    { "left_gui", 0xE3 },
    { "right_ctrl", 0xE4 },
    { "right_shift", 0xE5 },
    { "right_alt", 0xE6 },
    { "right_gui", 0xE7 },
    { "ctrl", KEY_GENERIC_MODIFIERS + 0 },
    { "shift", KEY_GENERIC_MODIFIERS + 1 },
    { "alt", KEY_GENERIC_MODIFIERS + 2 },
    { "gui", KEY_GENERIC_MODIFIERS + 3 },
};

static const NAMED_CODE m_Gamepad[] = {
    { "south", GAMEPAD_SOUTH },
    { "east", GAMEPAD_EAST },
    { "west", GAMEPAD_WEST },
    { "north", GAMEPAD_NORTH },
    { "left_shoulder", GAMEPAD_LEFT_SHOULDER },
    { "right_shoulder", GAMEPAD_RIGHT_SHOULDER },
    { "back", GAMEPAD_BACK },
    { "start", GAMEPAD_START },
    { "guide", GAMEPAD_GUIDE },
    { "left_stick_press", GAMEPAD_LEFT_STICK_PRESS },
    { "right_stick_press", GAMEPAD_RIGHT_STICK_PRESS },
    { "dpad_up", GAMEPAD_DPAD_UP },
    { "dpad_down", GAMEPAD_DPAD_DOWN },
    { "dpad_left", GAMEPAD_DPAD_LEFT },
    { "dpad_right", GAMEPAD_DPAD_RIGHT },
    { "left_trigger", GAMEPAD_LEFT_TRIGGER },
    { "right_trigger", GAMEPAD_RIGHT_TRIGGER },
    { "left_stick_up", GAMEPAD_LEFT_STICK_UP },
    { "left_stick_down", GAMEPAD_LEFT_STICK_DOWN },
    { "left_stick_left", GAMEPAD_LEFT_STICK_LEFT },
    { "left_stick_right", GAMEPAD_LEFT_STICK_RIGHT },
    { "right_stick_up", GAMEPAD_RIGHT_STICK_UP },
    { "right_stick_down", GAMEPAD_RIGHT_STICK_DOWN },
    { "right_stick_left", GAMEPAD_RIGHT_STICK_LEFT },
    { "right_stick_right", GAMEPAD_RIGHT_STICK_RIGHT },
    { "left_stick", GAMEPAD_LEFT_STICK },
    { "right_stick", GAMEPAD_RIGHT_STICK },
};

static const char *const m_HatDirNames[4] = { "up", "down", "left", "right" };

static const NAMED_CODE *M_Find(
    const NAMED_CODE *const table, const size_t count, const char *const name)
{
    for (size_t i = 0; i < count; i++) {
        if (Str_EqualIgnoreCase(table[i].name, name)) {
            return &table[i];
        }
    }
    return nullptr;
}

static const char *M_NameOf(
    const NAMED_CODE *const table, const size_t count, const uint16_t code)
{
    for (size_t i = 0; i < count; i++) {
        if (table[i].code == code) {
            return table[i].name;
        }
    }
    return nullptr;
}

static void M_FormatJoystickName(
    const uint16_t code, char *const out, const size_t size)
{
    const uint16_t tag = code & JOY_TAG_MASK;
    const uint16_t index = code & JOY_INDEX_MASK;
    switch (tag) {
    case JOY_TAG_BUTTON:
        snprintf(out, size, "joy_button_%u", index);
        return;
    case JOY_TAG_HAT_DIR: {
        const int hat = index / 4;
        const int dir = index % 4;
        if (hat == 0) {
            snprintf(out, size, "joy_hat_%s", m_HatDirNames[dir]);
        } else {
            snprintf(out, size, "joy_hat_%d_%s", hat, m_HatDirNames[dir]);
        }
        return;
    }
    case JOY_TAG_AXIS_DIR: {
        const int axis = index / 2;
        const int dir = index % 2;
        snprintf(
            out, size, "joy_axis_%d%s", axis,
            dir == JOY_AXIS_DIR_POSITIVE ? "+" : "-");
        return;
    }
    case JOY_TAG_AXIS:
        snprintf(out, size, "joy_axis_%u", index);
        return;
    default:
        out[0] = '\0';
        return;
    }
}

static bool M_ParseJoystickName(const char *const name, uint16_t *const out)
{
    int index;
    char extra;
    if (sscanf(name, "joy_button_%d%c", &index, &extra) == 1 && index >= 0
        && index <= JOY_INDEX_MASK) {
        *out = Joy_Button(index);
        return true;
    }
    if (strcmp(name, "joy_hat_up") == 0) {
        *out = Joy_HatDir(0, JOY_HAT_DIR_UP);
        return true;
    }
    if (strcmp(name, "joy_hat_down") == 0) {
        *out = Joy_HatDir(0, JOY_HAT_DIR_DOWN);
        return true;
    }
    if (strcmp(name, "joy_hat_left") == 0) {
        *out = Joy_HatDir(0, JOY_HAT_DIR_LEFT);
        return true;
    }
    if (strcmp(name, "joy_hat_right") == 0) {
        *out = Joy_HatDir(0, JOY_HAT_DIR_RIGHT);
        return true;
    }
    int hat;
    char dir_name[8];
    if (sscanf(name, "joy_hat_%d_%7s%c", &hat, dir_name, &extra) == 2) {
        for (int i = 0; i < 4; i++) {
            if (strcmp(dir_name, m_HatDirNames[i]) == 0) {
                *out = Joy_HatDir(hat, i);
                return true;
            }
        }
    }
    char sign;
    if (sscanf(name, "joy_axis_%d%c%c", &index, &sign, &extra) == 2
        && index >= 0 && (sign == '+' || sign == '-')) {
        *out = Joy_AxisDir(index, sign == '+' ? 1 : 0);
        return true;
    }
    if (sscanf(name, "joy_axis_%d%c", &index, &extra) == 1 && index >= 0) {
        *out = Joy_Axis(index);
        return true;
    }
    return false;
}

static bool M_PartName(
    const INPUT_DEVICE device, const uint16_t code, char *const out,
    const size_t size)
{
    const char *name = nullptr;
    switch (device) {
    case INPUT_DEVICE_KEYBOARD:
        name = M_NameOf(m_Keyboard, ARRAY_SIZE(m_Keyboard), code);
        break;
    case INPUT_DEVICE_GAMEPAD:
        name = M_NameOf(m_Gamepad, ARRAY_SIZE(m_Gamepad), code);
        break;
    case INPUT_DEVICE_JOYSTICK:
        M_FormatJoystickName(code, out, size);
        return out[0] != '\0';
    case INPUT_DEVICE_COUNT:
        break;
    }
    if (name == nullptr) {
        return false;
    }
    snprintf(out, size, "%s", name);
    return true;
}

static bool M_PartFromName(
    const char *const name, INPUT_DEVICE *const device, uint16_t *const code)
{
    const NAMED_CODE *found = M_Find(m_Keyboard, ARRAY_SIZE(m_Keyboard), name);
    if (found != nullptr) {
        *device = INPUT_DEVICE_KEYBOARD;
        *code = found->code;
        return true;
    }
    found = M_Find(m_Gamepad, ARRAY_SIZE(m_Gamepad), name);
    if (found != nullptr) {
        *device = INPUT_DEVICE_GAMEPAD;
        *code = found->code;
        return true;
    }
    if (Str_StartsWith(name, "joy_") && M_ParseJoystickName(name, code)) {
        *device = INPUT_DEVICE_JOYSTICK;
        return true;
    }
    return false;
}

// Returns whether the '+' at plus belongs to the part that starts at start,
// as in "joy_axis_2+", instead of separating two parts.
static bool M_PlusEndsPart(const char *const start, const char *const plus)
{
    static const char prefix[] = "joy_axis_";
    const size_t prefix_length = sizeof(prefix) - 1;
    if ((size_t)(plus - start) <= prefix_length
        || strncmp(start, prefix, prefix_length) != 0) {
        return false;
    }
    for (const char *p = start + prefix_length; p < plus; p++) {
        if (*p < '0' || *p > '9') {
            return false;
        }
    }
    return true;
}

static bool M_IsSpace(const char c)
{
    return c == ' ' || c == '\t';
}

void Input_BindingName(
    const INPUT_BINDING binding, char out[INPUT_BINDING_NAME_SIZE])
{
    size_t length = 0;
    out[0] = '\0';
    for (size_t i = 0; i < binding.count && i < INPUT_CHORD_MAX; i++) {
        char part[INPUT_BINDING_NAME_SIZE];
        if (!M_PartName(binding.device, binding.codes[i], part, sizeof(part))) {
            out[0] = '\0';
            return;
        }
        const int written = snprintf(
            out + length, INPUT_BINDING_NAME_SIZE - length, "%s%s",
            i > 0 ? "+" : "", part);
        if (written < 0
            || (size_t)written >= INPUT_BINDING_NAME_SIZE - length) {
            return;
        }
        length += (size_t)written;
    }
}

bool Input_BindingFromName(const char *const name, INPUT_BINDING *const out)
{
    INPUT_BINDING binding = { .count = 0 };
    const char *p = name;
    while (true) {
        while (M_IsSpace(*p)) {
            p++;
        }
        const char *const start = p;
        while (*p != '\0' && (*p != '+' || M_PlusEndsPart(start, p))) {
            p++;
        }
        const char *end = p;
        while (end > start && M_IsSpace(end[-1])) {
            end--;
        }
        char part[INPUT_BINDING_NAME_SIZE];
        const size_t length = (size_t)(end - start);
        if (binding.count == INPUT_CHORD_MAX || length >= sizeof(part)) {
            return false;
        }
        memcpy(part, start, length);
        part[length] = '\0';
        INPUT_DEVICE device;
        uint16_t code;
        if (!M_PartFromName(part, &device, &code)
            || (binding.count > 0 && device != binding.device)) {
            return false;
        }
        binding.device = device;
        binding.codes[binding.count++] = code;
        if (*p == '\0') {
            break;
        }
        p++;
    }
    *out = binding;
    return true;
}
