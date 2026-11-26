#include "th02/input.h"

#include "core/base/assert.h"

#include <string.h>

static const INPUT_ACTION m_Actions[] = {
    INPUT_BUTTON(
        TH02_INPUT, up, "up", "dpad_up, left_stick_up",
        "joy_hat_up, joy_axis_1-"),
    INPUT_BUTTON(
        TH02_INPUT, down, "down", "dpad_down, left_stick_down",
        "joy_hat_down, joy_axis_1+"),
    INPUT_BUTTON(
        TH02_INPUT, left, "left", "dpad_left, left_stick_left",
        "joy_hat_left, joy_axis_0-"),
    INPUT_BUTTON(
        TH02_INPUT, right, "right", "dpad_right, left_stick_right",
        "joy_hat_right, joy_axis_0+"),
    INPUT_BUTTON(TH02_INPUT, keypad_up, "keypad_8", nullptr, nullptr),
    INPUT_BUTTON(TH02_INPUT, keypad_down, "keypad_2", nullptr, nullptr),
    INPUT_BUTTON(TH02_INPUT, keypad_left, "keypad_4", nullptr, nullptr),
    INPUT_BUTTON(TH02_INPUT, keypad_right, "keypad_6", nullptr, nullptr),
    INPUT_BUTTON(TH02_INPUT, up_left, "keypad_7", nullptr, nullptr),
    INPUT_BUTTON(TH02_INPUT, up_right, "keypad_9", nullptr, nullptr),
    INPUT_BUTTON(TH02_INPUT, down_left, "keypad_1", nullptr, nullptr),
    INPUT_BUTTON(TH02_INPUT, down_right, "keypad_3", nullptr, nullptr),
    INPUT_BUTTON(TH02_INPUT, shot, "z", "south", "joy_button_0"),
    INPUT_BUTTON(TH02_INPUT, space, "space", nullptr, nullptr),
    INPUT_BUTTON(TH02_INPUT, bomb, "x", "east", "joy_button_1"),
    INPUT_BUTTON(TH02_INPUT, cancel, "escape", "back", "joy_button_3"),
    INPUT_BUTTON(
        TH02_INPUT, ok, "return, keypad_enter", "start", "joy_button_2"),
    INPUT_BUTTON(TH02_INPUT, q, "q", nullptr, nullptr),
    INPUT_BUTTON(
        TH02_INPUT, extra_group_0, "1, 2, 3, 4, 5, 6, 7", nullptr, nullptr),
    INPUT_BUTTON(
        TH02_INPUT, extra_group_3, "o, p, left_bracket, right_bracket, a, s, d",
        nullptr, nullptr),
    INPUT_BUTTON(
        TH02_INPUT, extra_group_5, "backslash, non_us_hash, c, v, b, n, m",
        nullptr, nullptr),
    INPUT_BUTTON(
        TH02_INPUT, extra_group_1,
        "8, 9, 0, minus, equals, international_3, backspace, tab", nullptr,
        nullptr),
    INPUT_BUTTON(
        TH02_INPUT, extra_group_2, "w, e, r, t, y, u, i", nullptr, nullptr),
    INPUT_BUTTON(
        TH02_INPUT, extra_group_4, "f, g, h, j, k, l, semicolon, apostrophe",
        nullptr, nullptr),
    INPUT_BUTTON(
        TH02_INPUT, extra_group_6,
        "comma, period, slash, non_us_backslash, international_1, "
        "international_4, page_up, page_down",
        nullptr, nullptr),
    INPUT_BUTTON(
        TH02_INPUT, extra_group_7, "insert, delete, home, end", nullptr,
        nullptr),
    INPUT_BUTTON(TH02_INPUT, logo_aliases, "grave", nullptr, nullptr),
};
const INPUT_SCHEMA g_Th02InputSchema =
    INPUT_SCHEMA_OF(TH02_INPUT, m_Actions, 1);

uint16_t GameInput_Sense(const TH02_INPUT *const sample)
{
    return (sample->up || sample->keypad_up ? GAME_KEY_UP : 0)
        | (sample->down || sample->keypad_down ? GAME_KEY_DOWN : 0)
        | (sample->left || sample->keypad_left ? GAME_KEY_LEFT : 0)
        | (sample->right || sample->keypad_right ? GAME_KEY_RIGHT : 0)
        | (sample->up_left ? GAME_KEY_UP_LEFT : 0)
        | (sample->up_right ? GAME_KEY_UP_RIGHT : 0)
        | (sample->down_left ? GAME_KEY_DOWN_LEFT : 0)
        | (sample->down_right ? GAME_KEY_DOWN_RIGHT : 0)
        | (sample->shot || sample->space ? GAME_KEY_SHOT : 0)
        | (sample->bomb ? GAME_KEY_BOMB : 0)
        | (sample->cancel ? GAME_KEY_CANCEL : 0)
        | (sample->ok ? GAME_KEY_OK : 0) | (sample->q ? GAME_KEY_Q : 0);
}

bool GameInput_LogoKey(const TH02_INPUT *const sample)
{
    return sample->up || sample->down || sample->left || sample->right
        || sample->shot || sample->space || sample->bomb || sample->cancel
        || sample->ok || sample->q || sample->extra_group_0
        || sample->extra_group_3 || sample->extra_group_5
        || sample->extra_group_1 || sample->extra_group_2
        || sample->extra_group_4 || sample->extra_group_6
        || sample->extra_group_7 || sample->logo_aliases;
}

bool GameInput_AnyWaitKey(const TH02_INPUT *const sample)
{
    return sample->shot || sample->bomb || sample->cancel || sample->ok
        || sample->extra_group_0 || sample->extra_group_3
        || sample->extra_group_5;
}

void GameInput_BeginWait(KEY_WAIT *const wait, const TH02_INPUT *const sample)
{
    memset(wait, 0, sizeof(*wait));
    wait->frames_left = 2;
    wait->pending = GameInput_AnyWaitKey(sample);
}

bool GameInput_AdvanceWait(KEY_WAIT *const wait, const TH02_INPUT *const sample)
{
    ASSERT(wait->phase >= KEY_WAIT_RELEASE && wait->phase <= KEY_WAIT_DONE);
    if (wait->phase == KEY_WAIT_DONE) {
        return true;
    }
    ASSERT(wait->frames_left == 1 || wait->frames_left == 2);
    wait->frames_left--;
    if (wait->frames_left != 0) {
        return false;
    }
    const bool held = GameInput_AnyWaitKey(sample);
    if (wait->phase == KEY_WAIT_RELEASE) {
        if (!wait->pending && !held) {
            wait->phase = KEY_WAIT_PRESS;
        }
    } else if (wait->pending || held) {
        wait->phase = KEY_WAIT_DONE;
        wait->pending = false;
        return true;
    }
    wait->frames_left = 2;
    wait->pending = held;
    return false;
}
