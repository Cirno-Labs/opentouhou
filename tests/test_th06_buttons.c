#include "th06/buttons.h"

#include "shell_fake.h"
#include "test.h"

#include "core/base/utils.h"

#include <stdint.h>
#include <string.h>

typedef struct {
    size_t offset;
    uint16_t bit;
} BIT_CASE;

TEST(word_round_trips_each_bit)
{
    static const BIT_CASE cases[] = {
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
    for (size_t i = 0; i < ARRAY_SIZE(cases); i++) {
        TH06_INPUT input = {};
        *(bool *)((uint8_t *)&input + cases[i].offset) = true;
        const uint16_t word = Buttons_ToWord(&input);
        CHECK_EQ_INT(word, cases[i].bit);

        const TH06_INPUT back = Buttons_FromWord(word);
        CHECK(*(const bool *)((const uint8_t *)&back + cases[i].offset));
        CHECK_EQ_INT(Buttons_ToWord(&back), cases[i].bit);
    }
}

// The INI keys and the pad configuration rows follow this order.
TEST(schema_keeps_the_action_order)
{
    CHECK_EQ_INT(g_InputSchema.count, 14);
    CHECK_EQ_INT(TH06_ACTION_shoot, 0);
    CHECK_EQ_INT(TH06_ACTION_focus, 2);
    CHECK_EQ_INT(TH06_ACTION_skip, 8);
    CHECK_EQ_INT(TH06_ACTION_confirm, 12);
    CHECK_EQ_INT(TH06_ACTION_move, 13);
    CHECK_EQ_STR(g_InputSchema.actions[0].name, "shoot");
    CHECK_EQ_STR(g_InputSchema.actions[2].name, "focus");
    CHECK_EQ_STR(g_InputSchema.actions[8].name, "skip");
    CHECK_EQ_STR(g_InputSchema.actions[12].name, "confirm");
    CHECK_EQ_STR(g_InputSchema.actions[13].name, "move");
    CHECK_EQ_STR(
        g_InputSchema.actions[2].defaults[INPUT_DEVICE_KEYBOARD],
        "left_shift, right_shift");
    CHECK_EQ_STR(
        g_InputSchema.actions[8].defaults[INPUT_DEVICE_GAMEPAD], "north");
}

TEST(from_word_leaves_the_stick_at_zero)
{
    const TH06_INPUT input = Buttons_FromWord(0x1fff);
    CHECK_EQ_INT(input.move_x, 0);
    CHECK_EQ_INT(input.move_y, 0);
}

TEST(update_reports_pressed_on_the_first_calc_down)
{
    memset(&g_Buttons, 0, sizeof(g_Buttons));
    TH06_INPUT input = {};
    input.shoot = true;
    Buttons_Update(&input);
    CHECK(g_Buttons.held.shoot);
    CHECK(g_Buttons.pressed.shoot);
    CHECK(g_Buttons.repeated.shoot);
    CHECK_EQ_INT(g_Buttons.held_frames, 0);

    Buttons_Update(&input);
    CHECK(g_Buttons.held.shoot);
    CHECK(!g_Buttons.pressed.shoot);
    CHECK_EQ_INT(g_Buttons.held_frames, 1);
}

TEST(update_releases_and_presses_again)
{
    memset(&g_Buttons, 0, sizeof(g_Buttons));
    TH06_INPUT down = {};
    down.shoot = true;
    const TH06_INPUT up = {};
    Buttons_Update(&down);
    Buttons_Update(&up);
    CHECK(!g_Buttons.held.shoot);
    CHECK(!g_Buttons.pressed.shoot);
    Buttons_Update(&down);
    CHECK(g_Buttons.pressed.shoot);
}

TEST(update_repeats_every_8_calcs_once_held_30)
{
    memset(&g_Buttons, 0, sizeof(g_Buttons));
    TH06_INPUT input = {};
    input.shoot = true;
    for (int calc = 0; calc <= 73; calc++) {
        Buttons_Update(&input);
        const bool should_repeat = calc == 0 || calc == 33 || calc == 41
            || calc == 49 || calc == 57 || calc == 65 || calc == 73;
        CHECK_EQ_INT(g_Buttons.repeated.shoot, should_repeat);
    }
    CHECK_EQ_INT(g_Buttons.held_frames, 33);
}

TEST(update_caps_held_frames_at_30_once_repeating)
{
    memset(&g_Buttons, 0, sizeof(g_Buttons));
    TH06_INPUT input = {};
    input.shoot = true;
    for (int calc = 0; calc < 60; calc++) {
        Buttons_Update(&input);
    }
    CHECK(g_Buttons.held_frames >= 30);
    CHECK(g_Buttons.held_frames <= 38);
}

TEST(clear_keeps_held_frames_and_counts_as_pressed_next)
{
    memset(&g_Buttons, 0, sizeof(g_Buttons));
    TH06_INPUT input = {};
    input.shoot = true;
    for (int calc = 0; calc < 5; calc++) {
        Buttons_Update(&input);
    }
    const int32_t held_frames = g_Buttons.held_frames;
    Buttons_Clear();
    CHECK_EQ_INT(g_Buttons.held_frames, held_frames);
    CHECK(!g_Buttons.held.shoot);
    CHECK_EQ_INT(g_Buttons.word, 0);

    Buttons_Update(&input);
    CHECK(g_Buttons.pressed.shoot);
}

TEST(replay_uses_the_saved_previous_word_after_live_input)
{
    memset(&g_Buttons, 0, sizeof(g_Buttons));
    g_Buttons.word = 1;
    g_Buttons.held_frames = 31;
    const TH06_INPUT live = { .menu = true };
    Buttons_Update(&live);
    CHECK_EQ_INT(g_Buttons.previous_word, 1);
    CHECK_EQ_INT(g_Buttons.held_frames, 0);
    Buttons_ApplyReplay(1);
    CHECK_EQ_INT(g_Buttons.previous_word, 1);
    CHECK_EQ_INT(g_Buttons.word, 9);
    CHECK(g_Buttons.pressed.menu);
    CHECK(!g_Buttons.pressed.shoot);
}

TEST(replay_updates_the_repeat_counter_against_the_same_snapshot)
{
    memset(&g_Buttons, 0, sizeof(g_Buttons));
    g_Buttons.word = 1;
    g_Buttons.held_frames = 31;
    const TH06_INPUT input = { .shoot = true };
    Buttons_Update(&input);
    CHECK_EQ_INT(g_Buttons.held_frames, 32);
    CHECK(!g_Buttons.repeated.shoot);
    Buttons_ApplyReplay(1);
    CHECK_EQ_INT(g_Buttons.held_frames, 33);
    CHECK(g_Buttons.repeated.shoot);
    CHECK(!g_Buttons.pressed.shoot);
}

TEST(masked_press_counts_partial_release_while_another_bit_stays_down)
{
    memset(&g_Buttons, 0, sizeof(g_Buttons));
    g_Buttons.word = 3;
    const TH06_INPUT input = { .shoot = true };
    Buttons_Update(&input);
    CHECK(Buttons_Pressed(3));
    CHECK(!Buttons_Pressed(1));
    CHECK(!Buttons_Pressed(2));
    CHECK(!g_Buttons.pressed.shoot);
    Buttons_Clear();
    CHECK_EQ_INT(g_Buttons.previous_word, 0);
}

TEST(pressed_or_repeated_counts_a_partial_release)
{
    memset(&g_Buttons, 0, sizeof(g_Buttons));
    g_Buttons.word = 0x1001;
    const TH06_INPUT input = { .confirm = true };
    Buttons_Update(&input);
    CHECK(Buttons_Repeated(0x1001));
    CHECK(!Buttons_Repeated(0x1000));
}

TEST(pressed_or_repeated_counts_a_held_bit_on_a_repeat)
{
    memset(&g_Buttons, 0, sizeof(g_Buttons));
    const TH06_INPUT input = { .up = true };
    int32_t count = 0;
    for (int calc = 0; calc <= 33; calc++) {
        Buttons_Update(&input);
        if (Buttons_Repeated(0x10)) {
            count++;
        }
        CHECK(!Buttons_Repeated(0x20));
    }
    CHECK_EQ_INT(count, 2);
}

static void M_SetDevice(const INPUT_DEVICE device, const TH06_INPUT input)
{
    FakeShell_SetDeviceInput(device, &input, sizeof(input));
}

// Resets the fake shell with no device down.
static void M_Reset(void)
{
    FakeShell_Reset();
    M_SetDevice(INPUT_DEVICE_KEYBOARD, (TH06_INPUT) {});
    M_SetDevice(INPUT_DEVICE_GAMEPAD, (TH06_INPUT) {});
    M_SetDevice(INPUT_DEVICE_JOYSTICK, (TH06_INPUT) {});
}

static void M_HoldPadShoot(const bool held)
{
    M_SetDevice(INPUT_DEVICE_GAMEPAD, (TH06_INPUT) { .shoot = held });
    const TH06_INPUT input = { .shoot = held, .focus = held };
    Buttons_Update(&input);
}

static void M_Bind(const size_t action, const char *const name)
{
    INPUT_BINDING *const binding =
        &g_FakeShell.bindings[action][g_FakeShell.binding_counts[action]++];
    CHECK(Input_BindingFromName(name, binding));
}

TEST(shared_pad_shoot_focuses_from_the_8th_calc)
{
    M_Reset();
    memset(&g_Buttons, 0, sizeof(g_Buttons));
    g_Buttons.shared_focus = true;
    for (int calc = 1; calc <= 7; calc++) {
        M_HoldPadShoot(true);
        CHECK(g_Buttons.held.shoot);
        CHECK(!g_Buttons.held.focus);
    }
    M_HoldPadShoot(true);
    CHECK(g_Buttons.held.focus);
}

TEST(shared_pad_shoot_counts_up_to_16)
{
    M_Reset();
    memset(&g_Buttons, 0, sizeof(g_Buttons));
    g_Buttons.shared_focus = true;
    for (int calc = 0; calc < 20; calc++) {
        M_HoldPadShoot(true);
    }
    CHECK_EQ_INT(g_Buttons.shot_held_frames, 16);
}

TEST(shared_pad_release_above_8_takes_8_off_without_focus)
{
    M_Reset();
    memset(&g_Buttons, 0, sizeof(g_Buttons));
    g_Buttons.shared_focus = true;
    g_Buttons.shot_held_frames = 16;
    M_HoldPadShoot(false);
    CHECK(!g_Buttons.held.focus);
    CHECK_EQ_INT(g_Buttons.shot_held_frames, 8);
    M_HoldPadShoot(false);
    CHECK_EQ_INT(g_Buttons.shot_held_frames, 0);
}

TEST(shared_pad_quick_repress_focuses_at_once)
{
    M_Reset();
    memset(&g_Buttons, 0, sizeof(g_Buttons));
    g_Buttons.shared_focus = true;
    g_Buttons.shot_held_frames = 16;
    M_HoldPadShoot(false);
    M_HoldPadShoot(true);
    CHECK(g_Buttons.held.focus);
    CHECK_EQ_INT(g_Buttons.shot_held_frames, 9);
}

TEST(shared_joystick_shoot_counts_like_the_gamepad)
{
    M_Reset();
    memset(&g_Buttons, 0, sizeof(g_Buttons));
    g_Buttons.shared_focus = true;
    g_Buttons.shot_held_frames = 7;
    M_SetDevice(INPUT_DEVICE_JOYSTICK, (TH06_INPUT) { .shoot = true });
    const TH06_INPUT input = { .shoot = true, .focus = true };
    Buttons_Update(&input);
    CHECK(g_Buttons.held.focus);
}

TEST(shared_keyboard_focus_is_immediate)
{
    M_Reset();
    memset(&g_Buttons, 0, sizeof(g_Buttons));
    g_Buttons.shared_focus = true;
    M_SetDevice(INPUT_DEVICE_KEYBOARD, (TH06_INPUT) { .focus = true });
    const TH06_INPUT input = { .focus = true };
    Buttons_Update(&input);
    CHECK(g_Buttons.held.focus);
}

TEST(unshared_focus_is_the_inputs)
{
    M_Reset();
    memset(&g_Buttons, 0, sizeof(g_Buttons));
    M_HoldPadShoot(true);
    CHECK(g_Buttons.held.focus);
    CHECK_EQ_INT(g_Buttons.shot_held_frames, 0);
}

TEST(read_shared_focus_finds_a_pad_binding_of_shoot_and_focus)
{
    FakeShell_Reset();
    M_Bind(TH06_ACTION_shoot, "z");
    M_Bind(TH06_ACTION_shoot, "joy_button_3");
    M_Bind(TH06_ACTION_focus, "z");
    memset(&g_Buttons, 0, sizeof(g_Buttons));
    g_Buttons.shared_focus = true;
    Buttons_ReadSharedFocus();
    CHECK(!g_Buttons.shared_focus);
    M_Bind(TH06_ACTION_focus, "joy_button_3");
    Buttons_ReadSharedFocus();
    CHECK(g_Buttons.shared_focus);
    FakeShell_Reset();
}
