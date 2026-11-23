#include "th01/input.h"

#include "input_backend_fake.h"
#include "test.h"

static INPUT_BINDING M_Binding(const char *const name)
{
    INPUT_BINDING result = {};
    CHECK(Input_BindingFromName(name, &result));
    return result;
}

static void M_DoubleTapPair(GAME_INPUT *const state)
{
    const TH01_INPUT pressed = { .shot = true, .strike = true };
    GameInput_SenseGame(state, &pressed, false);
    GameInput_SenseGame(state, &(TH01_INPUT) {}, false);
    GameInput_SenseGame(state, &pressed, false);
}

TEST(schema_keeps_arrow_and_keypad_sources_separate)
{
    INPUT_STATE *const input = Input_Create(&g_InputSchema, &g_HeapAllocator);
    TH01_INPUT raw;
    FakeInput_SetKey(0x52, true);
    Input_Update(input, &raw);
    CHECK(raw.up);
    CHECK(!raw.keypad_up);
    FakeInput_SetKey(0x60, true);
    FakeInput_SetKey(0x52, false);
    FakeInput_SetKey(0x1d, true);
    FakeInput_SetKey(0x28, true);
    Input_Update(input, &raw);
    CHECK(!raw.up);
    CHECK(raw.keypad_up && raw.shot && raw.ok);
    CHECK(!raw.strike && !raw.cancel);
    Input_Destroy(input);
}

TEST(schema_reads_pad_and_joystick_actions)
{
    INPUT_STATE *const input = Input_Create(&g_InputSchema, &g_HeapAllocator);
    TH01_INPUT raw;
    FakeInput_Connect(1, INPUT_DEVICE_GAMEPAD, "pad", "01");
    FakeInput_SetButton(1, 0, true);
    FakeInput_SetAxis(1, 0, -1.0f);
    Input_Update(input, &raw);
    CHECK(raw.shot && raw.left);
    CHECK(!raw.keypad_left);
    FakeInput_Disconnect(1);
    FakeInput_Connect(2, INPUT_DEVICE_JOYSTICK, "stick", "02");
    FakeInput_SetAxis(2, 1, 1.0f);
    FakeInput_SetButton(2, 1, true);
    Input_Update(input, &raw);
    CHECK(raw.down && raw.strike);
    CHECK(!raw.shot && !raw.left);
    Input_Destroy(input);
}

TEST(held_actions_do_not_reassert_consumed_flags)
{
    GAME_INPUT state = {};
    TH01_INPUT raw = { .shot = true, .strike = true, .ok = true };
    CHECK(!GameInput_SenseGame(&state, &raw, false));
    CHECK(state.shot && state.strike && state.ok);
    state.shot = false;
    state.strike = false;
    state.ok = false;
    GameInput_SenseGame(&state, &raw, false);
    CHECK(!state.shot && !state.strike && !state.ok);
    raw = (TH01_INPUT) {};
    GameInput_SenseGame(&state, &raw, false);
    raw.shot = true;
    GameInput_SenseGame(&state, &raw, false);
    CHECK(state.shot);
    CHECK(!state.strike && !state.ok);
}

TEST(changing_an_alias_reasserts_a_consumed_direction)
{
    GAME_INPUT state = {};
    TH01_INPUT raw = { .up = true, .left = true, .down = true, .right = true };
    GameInput_SenseGame(&state, &raw, false);
    CHECK(state.up && state.down);
    CHECK_EQ_INT(state.horizontal, GAME_INPUT_BOTH);
    state.up = false;
    state.down = false;
    state.horizontal = GAME_INPUT_NONE;
    GameInput_SenseGame(&state, &raw, false);
    CHECK(!state.up && !state.down);
    CHECK_EQ_INT(state.horizontal, GAME_INPUT_NONE);
    raw.keypad_up = raw.keypad_down = true;
    raw.keypad_left = raw.keypad_right = true;
    GameInput_SenseGame(&state, &raw, false);
    CHECK(state.up && state.down);
    CHECK_EQ_INT(state.horizontal, GAME_INPUT_BOTH);
    state.up = false;
    state.horizontal = GAME_INPUT_NONE;
    raw.up = raw.left = raw.down = raw.right = false;
    GameInput_SenseGame(&state, &raw, false);
    CHECK(state.up && state.down);
    CHECK_EQ_INT(state.horizontal, GAME_INPUT_BOTH);
    GameInput_SenseGame(&state, &(TH01_INPUT) {}, false);
    CHECK(!state.up && !state.down);
    CHECK_EQ_INT(state.horizontal, GAME_INPUT_NONE);
}

TEST(bomb_needs_two_presses_of_both_buttons)
{
    GAME_INPUT state = { .bomb_frame = GAME_INPUT_BOMB_WINDOW };
    M_DoubleTapPair(&state);
    CHECK(state.bomb);
    CHECK_EQ_INT(state.shot_presses, 0);
    CHECK_EQ_INT(state.strike_presses, 0);
    CHECK_EQ_INT(state.bomb_frame, 0);
    GameInput_SenseGame(&state, &(TH01_INPUT) {}, false);
    CHECK(state.bomb);
    GameInput_ResetGame(&state);
    CHECK(!state.bomb);
}

TEST(only_one_button_double_tapped_does_not_bomb)
{
    GAME_INPUT state = {};
    for (int32_t i = 0; i < 3; i++) {
        GameInput_SenseGame(&state, &(TH01_INPUT) { .shot = true }, false);
        GameInput_SenseGame(&state, &(TH01_INPUT) {}, false);
    }
    CHECK_EQ_INT(state.shot_presses, 3);
    CHECK_EQ_INT(state.strike_presses, 0);
    CHECK(!state.bomb);
}

TEST(bomb_window_boundary_and_simultaneous_order_match)
{
    GAME_INPUT state = { .shot_presses = 1,
                         .strike_presses = 1,
                         .bomb_frame = 19 };
    GameInput_SenseGame(
        &state, &(TH01_INPUT) { .shot = true, .strike = true }, false);
    CHECK(state.bomb);
    state = (GAME_INPUT) { .shot_presses = 1,
                           .strike_presses = 1,
                           .bomb_frame = 20 };
    GameInput_SenseGame(
        &state, &(TH01_INPUT) { .shot = true, .strike = true }, false);
    CHECK(!state.bomb);
    CHECK_EQ_INT(state.shot_presses, 1);
    CHECK_EQ_INT(state.strike_presses, 1);
    CHECK_EQ_INT(state.bomb_frame, 0);
    state = (GAME_INPUT) { .shot_presses = 5, .bomb_frame = 20 };
    GameInput_SenseGame(&state, &(TH01_INPUT) { .strike = true }, false);
    CHECK_EQ_INT(state.shot_presses, 0);
    CHECK_EQ_INT(state.strike_presses, 1);
}

TEST(sensing_does_not_age_the_window_and_advancement_wraps)
{
    GAME_INPUT state = { .bomb_frame = 19 };
    for (int32_t i = 0; i < 100; i++) {
        GameInput_SenseGame(&state, &(TH01_INPUT) {}, false);
    }
    CHECK_EQ_INT(state.bomb_frame, 19);
    GameInput_AdvanceBombFrame(&state);
    CHECK_EQ_INT(state.bomb_frame, 20);
    state.bomb_frame = INT16_MAX;
    GameInput_AdvanceBombFrame(&state);
    CHECK_EQ_INT(state.bomb_frame, INT16_MIN);
    state.shot_presses = UINT8_MAX;
    GameInput_SenseGame(&state, &(TH01_INPUT) { .shot = true }, false);
    CHECK_EQ_INT(state.shot_presses, 0);
}

TEST(opposing_directions_and_held_shot_trigger_a_persistent_bomb)
{
    GAME_INPUT state = {};
    const TH01_INPUT raw = { .left = true, .right = true, .shot = true };
    GameInput_SenseGame(&state, &raw, false);
    CHECK(state.bomb);
    state.bomb = false;
    GameInput_SenseGame(&state, &raw, false);
    CHECK(state.bomb);
    state.bomb = false;
    state.shot = false;
    GameInput_SenseGame(&state, &raw, false);
    CHECK(!state.bomb);
}

TEST(pause_precedes_return_and_uses_the_consumed_shot_flag)
{
    GAME_INPUT state = {};
    TH01_INPUT raw = { .cancel = true, .ok = true, .shot = true };
    CHECK(GameInput_SenseGame(&state, &raw, false));
    CHECK(state.paused && state.ok);
    CHECK(!GameInput_SenseGame(&state, &raw, false));
    CHECK(state.paused);
    raw.ok = false;
    GameInput_SenseGame(&state, &raw, false);
    state.shot = false;
    raw.ok = true;
    CHECK(!GameInput_SenseGame(&state, &raw, false));
    raw.cancel = false;
    GameInput_SenseGame(&state, &raw, false);
    raw.cancel = true;
    GameInput_SenseGame(&state, &raw, false);
    CHECK(!state.paused);
}

TEST(game_reset_keeps_bomb_frame_and_debug_key_history)
{
    GAME_INPUT state = {
        .previous = { .shot = true,
                      .memory_enter = true,
                      .memory_leave = true },
        .bomb_frame = 17,
        .shot_presses = 2,
        .strike_presses = 3,
        .shot = true,
        .strike = true,
        .paused = true,
        .bomb = true,
        .memory_enter = true,
        .memory_leave = true,
    };
    GameInput_ResetGame(&state);
    CHECK(!state.previous.shot);
    CHECK(!state.bomb);
    CHECK_EQ_INT(state.shot_presses, 0);
    CHECK_EQ_INT(state.strike_presses, 0);
    CHECK(!state.shot && !state.strike && !state.paused);
    CHECK(!state.memory_enter && !state.memory_leave);
    CHECK(state.previous.memory_enter && state.previous.memory_leave);
    CHECK_EQ_INT(state.bomb_frame, 17);
    GameInput_SenseGame(&state, &(TH01_INPUT) { .shot = true }, false);
    CHECK(state.shot);
}

TEST(menu_reset_keeps_held_history_and_other_flags)
{
    GAME_INPUT state = {};
    TH01_INPUT raw = {
        .left = true, .shot = true, .ok = true, .up = true, .strike = true
    };
    GameInput_SenseGame(&state, &raw, false);
    GameInput_ResetMenu(&state);
    CHECK(!state.shot && !state.ok);
    CHECK_EQ_INT(state.horizontal, GAME_INPUT_NONE);
    CHECK(state.up && state.strike);
    GameInput_SenseGame(&state, &raw, false);
    CHECK(!state.shot && !state.ok);
    CHECK_EQ_INT(state.horizontal, GAME_INPUT_NONE);
    raw.keypad_left = true;
    GameInput_SenseGame(&state, &raw, false);
    CHECK_EQ_INT(state.horizontal, GAME_INPUT_LEFT);
}

TEST(debug_history_survives_reset_and_is_ignored_outside_debug)
{
    GAME_INPUT state = {};
    TH01_INPUT raw = { .memory_enter = true, .memory_leave = true };
    GameInput_SenseGame(&state, &raw, false);
    CHECK(!state.memory_enter && !state.previous.memory_enter);
    GameInput_SenseGame(&state, &raw, true);
    CHECK(state.memory_enter && state.memory_leave);
    GameInput_ResetGame(&state);
    GameInput_SenseGame(&state, &raw, true);
    CHECK(!state.memory_enter && !state.memory_leave);
    GameInput_SenseGame(&state, &(TH01_INPUT) {}, true);
    GameInput_SenseGame(&state, &raw, true);
    CHECK(state.memory_enter && state.memory_leave);
}

TEST(menu_histories_are_independent_and_music_shares_options)
{
    MENU_INPUT state = {};
    TH01_INPUT raw = { .up = true };
    CHECK_EQ_INT(GameInput_SenseMenu(&state, &raw, MENU_INPUT_MAIN, 4, 0), 3);
    CHECK_EQ_INT(GameInput_SenseMenu(&state, &raw, MENU_INPUT_MAIN, 4, 3), 3);
    CHECK_EQ_INT(
        GameInput_SenseMenu(&state, &raw, MENU_INPUT_OPTIONS, 5, 0), 4);
    CHECK_EQ_INT(
        GameInput_SenseMenu(&state, &raw, MENU_INPUT_OPTIONS, 2, 0), 0);
    raw.keypad_up = true;
    raw.up = false;
    CHECK_EQ_INT(GameInput_SenseMenu(&state, &raw, MENU_INPUT_MAIN, 4, 3), 3);
    GameInput_SenseMenu(&state, &(TH01_INPUT) {}, MENU_INPUT_MAIN, 4, 3);
    CHECK_EQ_INT(GameInput_SenseMenu(&state, &raw, MENU_INPUT_MAIN, 4, 3), 2);
}

TEST(menu_navigation_updates_in_order_and_confirmation_stays_held)
{
    MENU_INPUT state = {};
    TH01_INPUT raw = { .up = true,
                       .down = true,
                       .left = true,
                       .keypad_right = true,
                       .ok = true,
                       .shot = true,
                       .cancel = true };
    CHECK_EQ_INT(
        GameInput_SenseMenu(&state, &raw, MENU_INPUT_OPTIONS, 5, 0), 0);
    CHECK(state.left && state.right && state.ok && state.shot && state.cancel);
    GameInput_SenseMenu(&state, &raw, MENU_INPUT_MAIN, 4, 0);
    CHECK(state.left && state.right && state.ok && state.shot && state.cancel);
    GameInput_SenseMenu(&state, &(TH01_INPUT) {}, MENU_INPUT_OPTIONS, 5, 0);
    CHECK(
        !state.left && !state.right && !state.ok && !state.shot
        && !state.cancel);
    CHECK_EQ_INT(
        GameInput_SenseMenu(
            &state, &(TH01_INPUT) { .down = true }, MENU_INPUT_MAIN, 4, 3),
        3);
}

TEST(intro_latches_unbound_keys_and_keeps_pending_presses)
{
    bool pending = false;
    INPUT_BINDING key = M_Binding("a");
    GameInput_LatchIntro(&pending, &(TH01_INPUT) {}, &key);
    CHECK(pending);
    GameInput_LatchIntro(&pending, &(TH01_INPUT) {}, nullptr);
    CHECK(pending);
    pending = false;
    key = M_Binding("f1");
    GameInput_LatchIntro(&pending, &(TH01_INPUT) {}, &key);
    CHECK(pending);
    pending = false;
    key = M_Binding("ctrl+z");
    GameInput_LatchIntro(&pending, &(TH01_INPUT) {}, &key);
    CHECK(pending);
    pending = false;
    GameInput_LatchIntro(&pending, &(TH01_INPUT) { .strike = true }, nullptr);
    CHECK(pending);
}

TEST(intro_ignores_modifier_and_lock_only_presses)
{
    static const uint16_t codes[] = {
        0,    0x39, 0x47, 0x53, 0xe0, 0xe1,  0xe2,
        0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0x100, 0x101
    };
    for (size_t i = 0; i < sizeof(codes) / sizeof(*codes); i++) {
        bool pending = false;
        const INPUT_BINDING key = { .device = INPUT_DEVICE_KEYBOARD,
                                    .count = 1,
                                    .codes = { codes[i] } };
        GameInput_LatchIntro(&pending, &(TH01_INPUT) {}, &key);
        CHECK(!pending);
    }
    bool pending = false;
    INPUT_BINDING invalid = { .device = INPUT_DEVICE_GAMEPAD,
                              .count = 1,
                              .codes = { 4 } };
    GameInput_LatchIntro(&pending, &(TH01_INPUT) {}, &invalid);
    CHECK(!pending);
    invalid = (INPUT_BINDING) { .device = INPUT_DEVICE_KEYBOARD,
                                .count = INPUT_CHORD_MAX + 1 };
    GameInput_LatchIntro(&pending, &(TH01_INPUT) {}, &invalid);
    CHECK(!pending);
}
