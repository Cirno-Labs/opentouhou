#include "th01/input.h"

#include "pc98/zun/math16.h"

#include "core/base/assert.h"
#include "core/input/codes.h"

#include <string.h>

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

GAME_INPUT g_GameInput;

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
    const bool first, const bool second, bool *const previous_first,
    bool *const previous_second, const uint8_t flag)
{
    if (M_ChangedPair(first, second, previous_first, previous_second)) {
        if (first || second) {
            g_GameInput.horizontal |= flag;
        } else {
            g_GameInput.horizontal &= (uint8_t)~flag;
        }
    }
}

static void M_DoubleTap(uint8_t *const pressed, uint8_t *const other)
{
    if (g_GameInput.bomb_frame < GAME_INPUT_BOMB_WINDOW) {
        *pressed = (uint8_t)(*pressed + 1);
    } else {
        g_GameInput.bomb_frame = 0;
        *pressed = 1;
        *other = 0;
    }
}

bool GameInput_SenseGame(const TH01_INPUT *const raw, const bool debug)
{
    TH01_INPUT *const previous = &g_GameInput.previous;
    if (M_ChangedPair(
            raw->up, raw->keypad_up, &previous->up, &previous->keypad_up)) {
        g_GameInput.up = raw->up || raw->keypad_up;
    }
    if (M_ChangedPair(
            raw->down, raw->keypad_down, &previous->down,
            &previous->keypad_down)) {
        g_GameInput.down = raw->down || raw->keypad_down;
    }
    M_Horizontal(
        raw->left, raw->keypad_left, &previous->left, &previous->keypad_left,
        GAME_INPUT_LEFT);
    M_Horizontal(
        raw->right, raw->keypad_right, &previous->right,
        &previous->keypad_right, GAME_INPUT_RIGHT);
    if (raw->shot != previous->shot) {
        g_GameInput.shot = raw->shot;
        if (raw->shot) {
            M_DoubleTap(&g_GameInput.shot_presses, &g_GameInput.strike_presses);
        }
    }
    previous->shot = raw->shot;
    if (raw->strike != previous->strike) {
        g_GameInput.strike = raw->strike;
        if (raw->strike) {
            M_DoubleTap(&g_GameInput.strike_presses, &g_GameInput.shot_presses);
        }
    }
    previous->strike = raw->strike;
    if (raw->cancel && !previous->cancel) {
        g_GameInput.paused = !g_GameInput.paused;
    }
    previous->cancel = raw->cancel;
    bool player_hit = false;
    if (raw->ok != previous->ok) {
        g_GameInput.ok = raw->ok;
        player_hit = raw->ok && g_GameInput.paused && g_GameInput.shot;
    }
    previous->ok = raw->ok;
    if ((g_GameInput.shot_presses >= 2 && g_GameInput.strike_presses >= 2)
        || (g_GameInput.horizontal == GAME_INPUT_BOTH && g_GameInput.shot)) {
        g_GameInput.bomb = true;
        g_GameInput.shot_presses = 0;
        g_GameInput.strike_presses = 0;
    }
    if (debug) {
        if (raw->memory_enter != previous->memory_enter) {
            g_GameInput.memory_enter = raw->memory_enter;
        }
        previous->memory_enter = raw->memory_enter;
        if (raw->memory_leave != previous->memory_leave) {
            g_GameInput.memory_leave = raw->memory_leave;
        }
        previous->memory_leave = raw->memory_leave;
    }
    return player_hit;
}

void GameInput_SenseEnding(const TH01_INPUT *const raw)
{
    TH01_INPUT *const previous = &g_GameInput.previous;
    if (raw->up != previous->up) {
        g_GameInput.up = raw->up;
    }
    if (raw->down != previous->down) {
        g_GameInput.down = raw->down;
    }
    if (raw->left != previous->left) {
        g_GameInput.horizontal = raw->left
            ? g_GameInput.horizontal | GAME_INPUT_LEFT
            : g_GameInput.horizontal & (uint8_t)~GAME_INPUT_LEFT;
    }
    if (raw->right != previous->right) {
        g_GameInput.horizontal = raw->right
            ? g_GameInput.horizontal | GAME_INPUT_RIGHT
            : g_GameInput.horizontal & (uint8_t)~GAME_INPUT_RIGHT;
    }
    if (raw->shot != previous->shot) {
        g_GameInput.shot = raw->shot;
    }
    if (raw->strike != previous->strike) {
        g_GameInput.strike = raw->strike;
    }
    if (raw->cancel && !previous->cancel) {
        g_GameInput.paused = !g_GameInput.paused;
    }
    if (raw->ok != previous->ok) {
        g_GameInput.ok = raw->ok;
    }
    previous->up = raw->up;
    previous->down = raw->down;
    previous->left = raw->left;
    previous->right = raw->right;
    previous->shot = raw->shot;
    previous->strike = raw->strike;
    previous->cancel = raw->cancel;
    previous->ok = raw->ok;
}

static void M_ResetRepeat(void)
{
    g_GameInput.previous = (TH01_INPUT) {
        .memory_enter = g_GameInput.previous.memory_enter,
        .memory_leave = g_GameInput.previous.memory_leave,
    };
    g_GameInput.shot_presses = 0;
    g_GameInput.strike_presses = 0;
    g_GameInput.bomb = false;
}

void GameInput_Init(void)
{
    memset(&g_GameInput, 0, sizeof(g_GameInput));
}

void GameInput_ResetGame(void)
{
    const TH01_INPUT previous = g_GameInput.previous;
    const int16_t bomb_frame = g_GameInput.bomb_frame;
    memset(&g_GameInput, 0, sizeof(g_GameInput));
    g_GameInput.previous = previous;
    g_GameInput.bomb_frame = bomb_frame;
    M_ResetRepeat();
}

void GameInput_ResetMenu(void)
{
    g_GameInput.horizontal = GAME_INPUT_NONE;
    g_GameInput.shot = false;
    g_GameInput.ok = false;
}

void GameInput_AdvanceBombFrame(void)
{
    g_GameInput.bomb_frame = Math16_Wrap(g_GameInput.bomb_frame + 1);
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
