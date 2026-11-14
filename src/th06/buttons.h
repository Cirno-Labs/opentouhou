#pragma once

#include "core/input.h"

#include <stdint.h>

// Lists th06's actions: each button with its bit in the button word, then
// the stick, with their default bindings on the keyboard, a gamepad and a
// joystick.
// clang-format off
#define TH06_ACTIONS(BUTTON, STICK)                                                                                                   \
    BUTTON(shoot,        0x1,    "z",                                   "south",                        "joy_button_0")               \
    BUTTON(bomb,         0x2,    "x",                                   "east",                         "joy_button_1")               \
    BUTTON(focus,        0x4,    "left_shift, right_shift",             "west",                         "joy_button_2")               \
    BUTTON(menu,         0x8,    "escape",                              "start",                        "joy_button_3")               \
    BUTTON(up,           0x10,   "up, keypad_8, keypad_7, keypad_9",    "dpad_up, left_stick_up",       "joy_hat_up, joy_axis_1-")    \
    BUTTON(down,         0x20,   "down, keypad_2, keypad_1, keypad_3",  "dpad_down, left_stick_down",   "joy_hat_down, joy_axis_1+")  \
    BUTTON(left,         0x40,   "left, keypad_4, keypad_7, keypad_1",  "dpad_left, left_stick_left",   "joy_hat_left, joy_axis_0-")  \
    BUTTON(right,        0x80,   "right, keypad_6, keypad_9, keypad_3", "dpad_right, left_stick_right", "joy_hat_right, joy_axis_0+") \
    BUTTON(skip,         0x100,  "left_ctrl, right_ctrl",               "north",                        "joy_button_4")               \
    BUTTON(quit,         0x200,  "q",                                   nullptr,                        nullptr)                      \
    BUTTON(unlock_end,   0x400,  "s",                                   nullptr,                        nullptr)                      \
    BUTTON(unlock_start, 0x800,  "home",                                nullptr,                        nullptr)                      \
    BUTTON(confirm,      0x1000, "return",                              nullptr,                        nullptr)                      \
    STICK(move,                                                         "left_stick",                   "joy_axis_0, joy_axis_1")
// clang-format on

typedef struct {
#define TH06_BUTTON_FIELD(field_, ...) bool field_;
#define TH06_STICK_FIELD(field_, ...)                                          \
    float field_##_x;                                                          \
    float field_##_y;
    TH06_ACTIONS(TH06_BUTTON_FIELD, TH06_STICK_FIELD)
#undef TH06_BUTTON_FIELD
#undef TH06_STICK_FIELD
} TH06_INPUT;

// The index of each action in the input schema, for Shell_GetBindings.
typedef enum {
#define TH06_ACTION_INDEX(field_, ...) TH06_ACTION_##field_,
    TH06_ACTIONS(TH06_ACTION_INDEX, TH06_ACTION_INDEX)
#undef TH06_ACTION_INDEX
} TH06_ACTION;

extern const INPUT_SCHEMA g_InputSchema;

// Holds a frame's buttons, as the original derives them from the raw
// input: what is down, what is new this calc and what counts as held
// for a menu repeat.
typedef struct {
    TH06_INPUT held; // down this calc
    TH06_INPUT pressed; // down this calc, up the calc before
    TH06_INPUT repeated; // pressed, or held on a calc that repeats
    uint16_t word;
    uint16_t previous_word;
    int32_t held_frames;
    // Holds whether focus shares a gamepad or joystick binding with shoot.
    // Buttons_Update then gives pad focus as the original gives it for one
    // pad button mapped to both: the pad's shoot counts its calcs held in
    // shot_held_frames, up to 16, and focuses from the 8th; a release takes
    // 8 off a count above 8 and sets a lower one to 0. Keyboard focus stays
    // immediate.
    bool shared_focus;
    int32_t shot_held_frames;
} BUTTONS;

// Sets shared_focus from the bindings that Shell_GetBindings gives.
void Buttons_ReadSharedFocus(BUTTONS *buttons);
// Reads input into the word of this calc, with the focus of shared_focus
// from Shell_GetDeviceInput.
void Buttons_Update(BUTTONS *buttons, const TH06_INPUT *input);
// Replaces the gameplay bits with replay_word and repeats the input
// update against the previous word saved by Buttons_Update.
void Buttons_ApplyReplay(BUTTONS *buttons, uint16_t replay_word);
// Returns whether the masked word is nonzero and changed this calc.
bool Buttons_WasPressed(const BUTTONS *buttons, uint16_t mask);
// Returns whether Buttons_WasPressed holds for mask, or a bit of mask is
// down on a calc that repeats.
bool Buttons_WasPressedOrRepeated(const BUTTONS *buttons, uint16_t mask);
// Clears the buttons but not held_frames and the focus delay, as a scene
// switch does.
void Buttons_Clear(BUTTONS *buttons);

// Converts to and from the button word that replays store.
uint16_t Buttons_ToWord(const TH06_INPUT *input);
TH06_INPUT Buttons_FromWord(uint16_t word);
