#include "th07/buttons.h"

#include "shell/game.h"
#include "core/base/utils.h"

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

static uint16_t M_Read(
    BUTTONS *const buttons, const TH07_INPUT *const input,
    const bool shoot_focuses)
{
    TH07_INPUT read = *input;
    if (shoot_focuses) {
        TH07_INPUT gamepad;
        TH07_INPUT joystick;
        Shell_GetDeviceInput(INPUT_DEVICE_GAMEPAD, &gamepad);
        Shell_GetDeviceInput(INPUT_DEVICE_JOYSTICK, &joystick);
        const bool shoot = gamepad.shoot || joystick.shoot;
        int32_t *const frames = &buttons->shot_held_frames;
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

static void M_StepRepeat(BUTTONS *const buttons, const bool same)
{
    buttons->repeat = false;
    if (same) {
        if (buttons->held_frames >= 30) {
            if (buttons->held_frames % 8 == 0) {
                buttons->repeat = true;
            }
            if (buttons->held_frames >= 38) {
                buttons->held_frames = 30;
            }
        }
        buttons->held_frames++;
    } else {
        buttons->held_frames = 0;
    }
}

void Buttons_Update(
    BUTTONS *const buttons, const TH07_INPUT *const input,
    const bool shoot_focuses)
{
    buttons->previous_word = buttons->word;
    buttons->word = M_Read(buttons, input, shoot_focuses);
    M_StepRepeat(buttons, buttons->previous_word == buttons->word);
}

void Buttons_Merge(
    BUTTONS *const buttons, const TH07_INPUT *const input,
    const bool shoot_focuses)
{
    buttons->word |= M_Read(buttons, input, shoot_focuses);
}

void Buttons_Play(BUTTONS *const buttons, const uint16_t word)
{
    buttons->previous_play_word = buttons->play_word;
    buttons->play_word = word;
    M_StepRepeat(buttons, buttons->previous_play_word == buttons->play_word);
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
        || ((buttons->word & mask) != 0 && buttons->repeat);
}

void Buttons_Clear(BUTTONS *const buttons)
{
    buttons->word = 0;
    buttons->previous_word = 0;
    buttons->repeat = false;
}
