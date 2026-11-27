#include "th07/buttons.h"

#include "core/base/utils.h"
#include "shell/game.h"

static const INPUT_ACTION m_Actions[] = {
#define M_BUTTON(field_, bit_, keyboard_, gamepad_, joystick_)                 \
    INPUT_BUTTON(TH07_INPUT, field_, keyboard_, gamepad_, joystick_),
#define M_STICK(field_, gamepad_, joystick_)                                   \
    INPUT_STICK(TH07_INPUT, field_, gamepad_, joystick_),
    TH07_ACTIONS(M_BUTTON, M_STICK)
#undef M_BUTTON
#undef M_STICK
};
const INPUT_SCHEMA g_InputSchema = INPUT_SCHEMA_OF(TH07_INPUT, m_Actions, 1);
BUTTONS g_Buttons;

uint16_t Buttons_ToWord(const TH07_INPUT *const input)
{
    uint16_t word = 0;
#define M_BIT(field_, ...)                                                     \
    if (input->field_) {                                                       \
        word |= TH07_BUTTON_##field_;                                          \
    }
#define M_NO_BIT(...)
    TH07_ACTIONS(M_BIT, M_NO_BIT)
#undef M_BIT
#undef M_NO_BIT
    return word;
}

TH07_INPUT Buttons_FromWord(const uint16_t word)
{
    TH07_INPUT input = {};
#define M_BIT(field_, ...) input.field_ = (word & TH07_BUTTON_##field_) != 0;
#define M_NO_BIT(...)
    TH07_ACTIONS(M_BIT, M_NO_BIT)
#undef M_BIT
#undef M_NO_BIT
    return input;
}

static uint16_t M_Read(const TH07_INPUT *const input, const bool shoot_focuses)
{
    TH07_INPUT read = *input;
    if (shoot_focuses) {
        TH07_INPUT gamepad;
        TH07_INPUT joystick;
        Shell_GetDeviceInput(INPUT_DEVICE_GAMEPAD, &gamepad);
        Shell_GetDeviceInput(INPUT_DEVICE_JOYSTICK, &joystick);
        const bool shoot = gamepad.shoot || joystick.shoot;
        int32_t *const frames = &g_Buttons.shot_held_frames;
        bool focus = false;
        if (shoot) {
            *frames = MIN(*frames + 1, 20);
            focus = *frames >= 10;
        } else if (*frames > 10) {
            *frames -= 10;
            focus = true;
        } else {
            *frames = 0;
        }
        read.focus = input->focus || focus;
    }
    return Buttons_ToWord(&read);
}

static void M_StepRepeat(const bool same)
{
    g_Buttons.repeat = false;
    if (same) {
        if (g_Buttons.held_frames >= 30) {
            if (g_Buttons.held_frames % 8 == 0) {
                g_Buttons.repeat = true;
            }
            if (g_Buttons.held_frames >= 38) {
                g_Buttons.held_frames = 30;
            }
        }
        g_Buttons.held_frames++;
    } else {
        g_Buttons.held_frames = 0;
    }
}

void Buttons_Update(const TH07_INPUT *const input, const bool shoot_focuses)
{
    g_Buttons.previous_word = g_Buttons.word;
    g_Buttons.word = M_Read(input, shoot_focuses);
    M_StepRepeat(g_Buttons.previous_word == g_Buttons.word);
}

void Buttons_Merge(const TH07_INPUT *const input, const bool shoot_focuses)
{
    g_Buttons.word |= M_Read(input, shoot_focuses);
}

void Buttons_Play(const uint16_t word)
{
    g_Buttons.previous_play_word = g_Buttons.play_word;
    g_Buttons.play_word = word;
    M_StepRepeat(g_Buttons.previous_play_word == g_Buttons.play_word);
}

bool Buttons_Pressed(const uint16_t mask)
{
    return (g_Buttons.word & mask) != 0
        && (g_Buttons.word & mask) != (g_Buttons.previous_word & mask);
}

bool Buttons_Repeated(const uint16_t mask)
{
    return Buttons_Pressed(mask)
        || ((g_Buttons.word & mask) != 0 && g_Buttons.repeat);
}

void Buttons_Clear(void)
{
    g_Buttons.word = 0;
    g_Buttons.previous_word = 0;
    g_Buttons.repeat = false;
}
