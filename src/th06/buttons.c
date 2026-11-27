#include "th06/buttons.h"

#include "core/base/utils.h"
#include "shell/game.h"

#include <stddef.h>
#include <string.h>

enum {
    M_MAX_BINDINGS = 16,
};

static const INPUT_ACTION m_Actions[] = {
#define M_BUTTON(field_, bit_, keyboard_, gamepad_, joystick_)                 \
    INPUT_BUTTON(TH06_INPUT, field_, keyboard_, gamepad_, joystick_),
#define M_STICK(field_, gamepad_, joystick_)                                   \
    INPUT_STICK(TH06_INPUT, field_, gamepad_, joystick_),
    TH06_ACTIONS(M_BUTTON, M_STICK)
#undef M_BUTTON
#undef M_STICK
};
const INPUT_SCHEMA g_InputSchema = INPUT_SCHEMA_OF(TH06_INPUT, m_Actions, 1);
BUTTONS g_Buttons;

static void M_Update(void)
{
    const uint16_t previous = g_Buttons.previous_word;
    bool repeat = false;
    if (previous == g_Buttons.word) {
        if (g_Buttons.held_frames >= 30) {
            if (g_Buttons.held_frames % 8 == 0) {
                repeat = true;
            }
            if (g_Buttons.held_frames >= 38) {
                g_Buttons.held_frames = 30;
            }
        }
        g_Buttons.held_frames++;
    } else {
        g_Buttons.held_frames = 0;
    }

    const uint16_t pressed = g_Buttons.word & (uint16_t)~previous;
    const uint16_t repeated = pressed | (repeat ? g_Buttons.word : 0);
    g_Buttons.held = Buttons_FromWord(g_Buttons.word);
    g_Buttons.pressed = Buttons_FromWord(pressed);
    g_Buttons.repeated = Buttons_FromWord(repeated);
}

uint16_t Buttons_ToWord(const TH06_INPUT *const input)
{
    uint16_t word = 0;
#define M_BIT(field_, ...)                                                     \
    if (input->field_) {                                                       \
        word |= TH06_BUTTON_##field_;                                          \
    }
#define M_NO_BIT(...)
    TH06_ACTIONS(M_BIT, M_NO_BIT)
#undef M_BIT
#undef M_NO_BIT
    return word;
}

TH06_INPUT Buttons_FromWord(const uint16_t word)
{
    TH06_INPUT input = {};
#define M_BIT(field_, ...) input.field_ = (word & TH06_BUTTON_##field_) != 0;
#define M_NO_BIT(...)
    TH06_ACTIONS(M_BIT, M_NO_BIT)
#undef M_BIT
#undef M_NO_BIT
    return input;
}

static bool M_SamePadBinding(const INPUT_BINDING a, const INPUT_BINDING b)
{
    return a.device != INPUT_DEVICE_KEYBOARD && a.device == b.device
        && a.count == b.count
        && memcmp(a.codes, b.codes, a.count * sizeof(a.codes[0])) == 0;
}

void Buttons_ReadSharedFocus(void)
{
    INPUT_BINDING shoot[M_MAX_BINDINGS];
    INPUT_BINDING focus[M_MAX_BINDINGS];
    const size_t shoot_count =
        MIN(Shell_GetBindings(TH06_ACTION_shoot, shoot, M_MAX_BINDINGS),
            (size_t)M_MAX_BINDINGS);
    const size_t focus_count =
        MIN(Shell_GetBindings(TH06_ACTION_focus, focus, M_MAX_BINDINGS),
            (size_t)M_MAX_BINDINGS);
    g_Buttons.shared_focus = false;
    for (size_t i = 0; i < focus_count; i++) {
        for (size_t j = 0; j < shoot_count; j++) {
            g_Buttons.shared_focus |= M_SamePadBinding(focus[i], shoot[j]);
        }
    }
}

void Buttons_Update(const TH06_INPUT *const input)
{
    TH06_INPUT read = *input;
    if (g_Buttons.shared_focus) {
        TH06_INPUT keyboard;
        TH06_INPUT gamepad;
        TH06_INPUT joystick;
        Shell_GetDeviceInput(INPUT_DEVICE_KEYBOARD, &keyboard);
        Shell_GetDeviceInput(INPUT_DEVICE_GAMEPAD, &gamepad);
        Shell_GetDeviceInput(INPUT_DEVICE_JOYSTICK, &joystick);
        const bool shoot = gamepad.shoot || joystick.shoot;
        int32_t *const frames = &g_Buttons.shot_held_frames;
        if (shoot) {
            *frames = MIN(*frames + 1, 16);
        } else {
            *frames = *frames > 8 ? *frames - 8 : 0;
        }
        read.focus = keyboard.focus || (shoot && *frames >= 8);
    }
    g_Buttons.previous_word = g_Buttons.word;
    g_Buttons.word = Buttons_ToWord(&read);
    M_Update();
    g_Buttons.held.move_x = input->move_x;
    g_Buttons.held.move_y = input->move_y;
}

void Buttons_ApplyReplay(const uint16_t replay_word)
{
    g_Buttons.word =
        (g_Buttons.word & (uint16_t)~TH06_BUTTON_recorded) | replay_word;
    M_Update();
}

bool Buttons_Pressed(const uint16_t mask)
{
    return (g_Buttons.word & mask) != 0
        && (g_Buttons.word & mask) != (g_Buttons.previous_word & mask);
}

bool Buttons_Repeated(const uint16_t mask)
{
    return Buttons_Pressed(mask)
        || (Buttons_ToWord(&g_Buttons.repeated) & mask) != 0;
}

void Buttons_Clear(void)
{
    g_Buttons.held = (TH06_INPUT) {};
    g_Buttons.pressed = (TH06_INPUT) {};
    g_Buttons.repeated = (TH06_INPUT) {};
    g_Buttons.word = 0;
    g_Buttons.previous_word = 0;
}
