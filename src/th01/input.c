#include "th01/input.h"

#include "th01/math.h"

#include "core/base/assert.h"
#include "core/input/codes.h"

static const INPUT_ACTION m_Actions[] = {
    INPUT_BUTTON(
        TH01_INPUT, up, "up", "dpad_up, left_stick_up",
        "joy_hat_up, joy_axis_1-"),
    INPUT_BUTTON(
        TH01_INPUT, down, "down", "dpad_down, left_stick_down",
        "joy_hat_down, joy_axis_1+"),
    INPUT_BUTTON(
        TH01_INPUT, left, "left", "dpad_left, left_stick_left",
        "joy_hat_left, joy_axis_0-"),
    INPUT_BUTTON(
        TH01_INPUT, right, "right", "dpad_right, left_stick_right",
        "joy_hat_right, joy_axis_0+"),
    INPUT_BUTTON(TH01_INPUT, keypad_up, "keypad_8", nullptr, nullptr),
    INPUT_BUTTON(TH01_INPUT, keypad_down, "keypad_2", nullptr, nullptr),
    INPUT_BUTTON(TH01_INPUT, keypad_left, "keypad_4", nullptr, nullptr),
    INPUT_BUTTON(TH01_INPUT, keypad_right, "keypad_6", nullptr, nullptr),
    INPUT_BUTTON(TH01_INPUT, shot, "z", "south", "joy_button_0"),
    INPUT_BUTTON(TH01_INPUT, strike, "x", "east", "joy_button_1"),
    INPUT_BUTTON(
        TH01_INPUT, ok, "return, keypad_enter", "start", "joy_button_2"),
    INPUT_BUTTON(TH01_INPUT, cancel, "escape", "back", "joy_button_3"),
    INPUT_BUTTON(TH01_INPUT, memory_enter, "page_up", nullptr, nullptr),
    INPUT_BUTTON(TH01_INPUT, memory_leave, "page_down", nullptr, nullptr),
};

const INPUT_SCHEMA g_InputSchema = INPUT_SCHEMA_OF(TH01_INPUT, m_Actions, 1);

static bool M_ChangedPair(
    const bool first, const bool second, bool *const previous_first,
    bool *const previous_second)
{
    const bool changed = first != *previous_first || second != *previous_second;
    *previous_first = first;
    *previous_second = second;
    return changed;
}

static void M_Horizontal(
    GAME_INPUT *const state, const bool first, const bool second,
    bool *const previous_first, bool *const previous_second, const uint8_t flag)
{
    if (M_ChangedPair(first, second, previous_first, previous_second)) {
        if (first || second) {
            state->horizontal |= flag;
        } else {
            state->horizontal &= (uint8_t)~flag;
        }
    }
}

static void M_DoubleTap(
    GAME_INPUT *const state, uint8_t *const pressed, uint8_t *const other)
{
    if (state->bomb_frame < GAME_INPUT_BOMB_WINDOW) {
        *pressed = (uint8_t)(*pressed + 1);
    } else {
        state->bomb_frame = 0;
        *pressed = 1;
        *other = 0;
    }
}

bool GameInput_SenseGame(
    GAME_INPUT *const state, const TH01_INPUT *const raw, const bool debug)
{
    TH01_INPUT *const previous = &state->previous;
    if (M_ChangedPair(
            raw->up, raw->keypad_up, &previous->up, &previous->keypad_up)) {
        state->up = raw->up || raw->keypad_up;
    }
    if (M_ChangedPair(
            raw->down, raw->keypad_down, &previous->down,
            &previous->keypad_down)) {
        state->down = raw->down || raw->keypad_down;
    }
    M_Horizontal(
        state, raw->left, raw->keypad_left, &previous->left,
        &previous->keypad_left, GAME_INPUT_LEFT);
    M_Horizontal(
        state, raw->right, raw->keypad_right, &previous->right,
        &previous->keypad_right, GAME_INPUT_RIGHT);
    if (raw->shot != previous->shot) {
        state->shot = raw->shot;
        if (raw->shot) {
            M_DoubleTap(state, &state->shot_presses, &state->strike_presses);
        }
    }
    previous->shot = raw->shot;
    if (raw->strike != previous->strike) {
        state->strike = raw->strike;
        if (raw->strike) {
            M_DoubleTap(state, &state->strike_presses, &state->shot_presses);
        }
    }
    previous->strike = raw->strike;
    if (raw->cancel && !previous->cancel) {
        state->paused = !state->paused;
    }
    previous->cancel = raw->cancel;
    bool player_hit = false;
    if (raw->ok != previous->ok) {
        state->ok = raw->ok;
        player_hit = raw->ok && state->paused && state->shot;
    }
    previous->ok = raw->ok;
    if ((state->shot_presses >= 2 && state->strike_presses >= 2)
        || (state->horizontal == GAME_INPUT_BOTH && state->shot)) {
        state->bomb = true;
        state->shot_presses = 0;
        state->strike_presses = 0;
    }
    if (debug) {
        if (raw->memory_enter != previous->memory_enter) {
            state->memory_enter = raw->memory_enter;
        }
        previous->memory_enter = raw->memory_enter;
        if (raw->memory_leave != previous->memory_leave) {
            state->memory_leave = raw->memory_leave;
        }
        previous->memory_leave = raw->memory_leave;
    }
    return player_hit;
}

void GameInput_ResetRepeat(GAME_INPUT *const state)
{
    state->previous = (TH01_INPUT) {
        .memory_enter = state->previous.memory_enter,
        .memory_leave = state->previous.memory_leave,
    };
    state->shot_presses = 0;
    state->strike_presses = 0;
    state->bomb = false;
}

void GameInput_ResetGame(GAME_INPUT *const state)
{
    const TH01_INPUT previous = state->previous;
    const int16_t bomb_frame = state->bomb_frame;
    *state = (GAME_INPUT) { .previous = previous, .bomb_frame = bomb_frame };
    GameInput_ResetRepeat(state);
}

void GameInput_ResetMenu(GAME_INPUT *const state)
{
    state->horizontal = GAME_INPUT_NONE;
    state->shot = false;
    state->ok = false;
}

void GameInput_AdvanceBombFrame(GAME_INPUT *const state)
{
    state->bomb_frame = Math_Wrap16(state->bomb_frame + 1);
}

int32_t GameInput_SenseMenu(
    MENU_INPUT *const state, const TH01_INPUT *const raw,
    const MENU_INPUT_KIND kind, const int32_t choice_count, int32_t selection)
{
    ASSERT(kind == MENU_INPUT_MAIN || kind == MENU_INPUT_OPTIONS);
    ASSERT(choice_count > 0 && selection >= 0 && selection < choice_count);
    const bool up = raw->up || raw->keypad_up;
    const bool down = raw->down || raw->keypad_down;
    if (up && !state->previous_up[kind]) {
        selection = selection == 0 ? choice_count - 1 : selection - 1;
    }
    state->previous_up[kind] = up;
    if (down && !state->previous_down[kind]) {
        selection = selection == choice_count - 1 ? 0 : selection + 1;
    }
    state->previous_down[kind] = down;
    if (kind == MENU_INPUT_OPTIONS) {
        state->left = raw->left || raw->keypad_left;
        state->right = raw->right || raw->keypad_right;
    }
    state->ok = raw->ok;
    state->shot = raw->shot;
    state->cancel = raw->cancel;
    return selection;
}

void GameInput_LatchIntro(
    bool *const pending, const TH01_INPUT *const raw,
    const INPUT_BINDING *const key_press)
{
    if (raw->shot || raw->strike || raw->ok || raw->cancel) {
        *pending = true;
    }
    if (key_press == nullptr || key_press->device != INPUT_DEVICE_KEYBOARD
        || key_press->count == 0 || key_press->count > INPUT_CHORD_MAX) {
        return;
    }
    const uint16_t trigger = key_press->codes[key_press->count - 1];
    if (trigger != 0 && trigger < 256
        && (trigger < KEY_LEFT_MODIFIERS
            || trigger >= KEY_RIGHT_MODIFIERS + KEY_MODIFIER_COUNT)
        && trigger != 0x39 && trigger != 0x47 && trigger != 0x53) {
        *pending = true;
    }
}
