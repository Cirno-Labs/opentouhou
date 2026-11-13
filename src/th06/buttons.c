// SPDX-License-Identifier: GPL-3.0-only

#include "th06/buttons.h"

#include "core/base/utils.h"

#include <stddef.h>

static const INPUT_ACTION m_Actions[] = {
    INPUT_BUTTON(TH06_INPUT, shoot, "z", "south", "joy_button_0"),
    INPUT_BUTTON(TH06_INPUT, bomb, "x", "east", "joy_button_1"),
    INPUT_BUTTON(
        TH06_INPUT, focus, "left_shift, right_shift", "west", "joy_button_2"),
    INPUT_BUTTON(TH06_INPUT, menu, "escape", "start", "joy_button_3"),
    INPUT_BUTTON(
        TH06_INPUT, up, "up, keypad_8, keypad_7, keypad_9",
        "dpad_up, left_stick_up", "joy_hat_up, joy_axis_1-"),
    INPUT_BUTTON(
        TH06_INPUT, down, "down, keypad_2, keypad_1, keypad_3",
        "dpad_down, left_stick_down", "joy_hat_down, joy_axis_1+"),
    INPUT_BUTTON(
        TH06_INPUT, left, "left, keypad_4, keypad_7, keypad_1",
        "dpad_left, left_stick_left", "joy_hat_left, joy_axis_0-"),
    INPUT_BUTTON(
        TH06_INPUT, right, "right, keypad_6, keypad_9, keypad_3",
        "dpad_right, left_stick_right", "joy_hat_right, joy_axis_0+"),
    INPUT_BUTTON(
        TH06_INPUT, skip, "left_ctrl, right_ctrl", "north", "joy_button_4"),
    INPUT_BUTTON(TH06_INPUT, quit, "q", nullptr, nullptr),
    INPUT_BUTTON(TH06_INPUT, unlock_end, "s", nullptr, nullptr),
    INPUT_BUTTON(TH06_INPUT, unlock_start, "home", nullptr, nullptr),
    INPUT_BUTTON(TH06_INPUT, confirm, "return", nullptr, nullptr),
    INPUT_STICK(TH06_INPUT, move, "left_stick", "joy_axis_0, joy_axis_1"),
};
const INPUT_SCHEMA g_InputSchema = INPUT_SCHEMA_OF(TH06_INPUT, m_Actions, 1);

typedef struct {
    size_t offset;
    uint16_t bit;
} BUTTON_BIT;

// The only place the button word's bit layout appears.
static const BUTTON_BIT m_Bits[] = {
    { offsetof(TH06_INPUT, shoot), 0x1 },
    { offsetof(TH06_INPUT, bomb), 0x2 },
    { offsetof(TH06_INPUT, focus), 0x4 },
    { offsetof(TH06_INPUT, menu), 0x8 },
    { offsetof(TH06_INPUT, up), 0x10 },
    { offsetof(TH06_INPUT, down), 0x20 },
    { offsetof(TH06_INPUT, left), 0x40 },
    { offsetof(TH06_INPUT, right), 0x80 },
    { offsetof(TH06_INPUT, skip), 0x100 },
    { offsetof(TH06_INPUT, quit), 0x200 },
    { offsetof(TH06_INPUT, unlock_end), 0x400 },
    { offsetof(TH06_INPUT, unlock_start), 0x800 },
    { offsetof(TH06_INPUT, confirm), 0x1000 },
};

static void M_Update(BUTTONS *const buttons)
{
    const uint16_t previous = buttons->previous_word;
    bool repeat = false;
    if (previous == buttons->word) {
        if (buttons->held_frames >= 30) {
            if (buttons->held_frames % 8 == 0) {
                repeat = true;
            }
            if (buttons->held_frames >= 38) {
                buttons->held_frames = 30;
            }
        }
        buttons->held_frames++;
    } else {
        buttons->held_frames = 0;
    }

    const uint16_t pressed = buttons->word & (uint16_t)~previous;
    const uint16_t repeated = pressed | (repeat ? buttons->word : 0);
    buttons->held = Buttons_FromWord(buttons->word);
    buttons->pressed = Buttons_FromWord(pressed);
    buttons->repeated = Buttons_FromWord(repeated);
}

uint16_t Buttons_ToWord(const TH06_INPUT *const input)
{
    const uint8_t *const base = (const uint8_t *)input;
    uint16_t word = 0;
    for (size_t i = 0; i < ARRAY_SIZE(m_Bits); i++) {
        if (*(const bool *)(base + m_Bits[i].offset)) {
            word |= m_Bits[i].bit;
        }
    }
    return word;
}

TH06_INPUT Buttons_FromWord(const uint16_t word)
{
    TH06_INPUT input = {};
    uint8_t *const base = (uint8_t *)&input;
    for (size_t i = 0; i < ARRAY_SIZE(m_Bits); i++) {
        *(bool *)(base + m_Bits[i].offset) = (word & m_Bits[i].bit) != 0;
    }
    return input;
}

void Buttons_Update(BUTTONS *const buttons, const TH06_INPUT *const input)
{
    buttons->previous_word = buttons->word;
    buttons->word = Buttons_ToWord(input);
    M_Update(buttons);
    buttons->held.move_x = input->move_x;
    buttons->held.move_y = input->move_y;
}

void Buttons_ApplyReplay(BUTTONS *const buttons, const uint16_t replay_word)
{
    buttons->word = (buttons->word & (uint16_t)~0x1f7) | replay_word;
    M_Update(buttons);
}

bool Buttons_WasPressed(const BUTTONS *const buttons, const uint16_t mask)
{
    return (buttons->word & mask) != 0
        && (buttons->word & mask) != (buttons->previous_word & mask);
}

bool Buttons_WasPressedOrRepeated(
    const BUTTONS *const buttons, const uint16_t mask)
{
    return Buttons_WasPressed(buttons, mask)
        || (Buttons_ToWord(&buttons->repeated) & mask) != 0;
}

void Buttons_Clear(BUTTONS *const buttons)
{
    buttons->held = (TH06_INPUT) {};
    buttons->pressed = (TH06_INPUT) {};
    buttons->repeated = (TH06_INPUT) {};
    buttons->word = 0;
    buttons->previous_word = 0;
}
