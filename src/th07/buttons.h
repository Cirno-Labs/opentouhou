#pragma once

#include "core/input.h"

#include <stdint.h>

// Lists th07's actions: each button with its bit in the button word, then
// the stick, with their default bindings on the keyboard, a gamepad and a
// joystick. cheat enters the score screen's cheat sequence; retry restarts
// from the pause menu.
// clang-format off
#define TH07_ACTIONS(BUTTON, STICK)                                                                                                   \
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
    BUTTON(cheat,        0x2000, "d",                                   nullptr,                        "joy_button_7")               \
    BUTTON(retry,        0x4000, "r",                                   nullptr,                        nullptr)                      \
    STICK(move,                                                         "left_stick",                   "joy_axis_0, joy_axis_1")
// clang-format on

typedef struct {
#define TH07_BUTTON_FIELD(field_, ...) bool field_;
#define TH07_STICK_FIELD(field_, ...)                                          \
    float field_##_x;                                                          \
    float field_##_y;
    TH07_ACTIONS(TH07_BUTTON_FIELD, TH07_STICK_FIELD)
#undef TH07_BUTTON_FIELD
#undef TH07_STICK_FIELD
} TH07_INPUT;

// The index of each action in the input schema, for Shell_GetBindings.
typedef enum {
#define TH07_ACTION_INDEX(field_, ...) TH07_ACTION_##field_,
    TH07_ACTIONS(TH07_ACTION_INDEX, TH07_ACTION_INDEX)
#undef TH07_ACTION_INDEX
} TH07_ACTION;

extern const INPUT_SCHEMA g_InputSchema;

// Holds a frame's buttons, as the original derives them from the raw
// input: what is down, what is new this calc and what counts as held
// for a menu repeat. Gameplay acts on play_word, which the replay sets
// from the live word or from the recording.
typedef struct {
    TH07_INPUT held; // down this calc
    TH07_INPUT pressed; // down this calc, up the calc before
    TH07_INPUT repeated; // pressed, or held on a calc that repeats
    uint16_t word;
    uint16_t previous_word;
    uint16_t play_word;
    uint16_t previous_play_word;
    int32_t held_frames;
    bool repeat; // this calc repeats
    // Counts a pad's held shoot, for the focus that shoot_focuses gives.
    int32_t shot_held_frames;
} BUTTONS;

// Reads input into the word, then updates what is pressed and repeated.
// With shoot_focuses, a pad's held shoot also focuses, from the gamepad
// and joystick input of Shell_GetDeviceInput.
void Buttons_Update(
    BUTTONS *buttons, const TH07_INPUT *input, bool shoot_focuses);

// Adds the buttons of input to the word, without a new previous word or
// repeat, as the original does on the calc after one that slow mode drops.
// The shoot count steps as in Buttons_Update.
void Buttons_Merge(
    BUTTONS *buttons, const TH07_INPUT *input, bool shoot_focuses);

// Sets the play word to word, and steps held_frames and repeat on it as
// Buttons_Update does on the live word, as replay playback does.
void Buttons_Play(BUTTONS *buttons, uint16_t word);

// Returns whether the masked word is nonzero and changed this calc.
bool Buttons_WasPressed(const BUTTONS *buttons, uint16_t mask);

// Returns whether Buttons_WasPressed holds for mask, or a bit of mask is
// down on a calc that repeats.
bool Buttons_WasPressedOrRepeated(const BUTTONS *buttons, uint16_t mask);

// Clears the words, the repeat and what derives from them, as a scene
// switch does. held_frames and shot_held_frames stay.
void Buttons_Clear(BUTTONS *buttons);

// Converts to and from the button word that replays store.
uint16_t Buttons_ToWord(const TH07_INPUT *input);
TH07_INPUT Buttons_FromWord(uint16_t word);
