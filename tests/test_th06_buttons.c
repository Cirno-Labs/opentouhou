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

TEST(from_word_leaves_the_stick_at_zero)
{
    const TH06_INPUT input = Buttons_FromWord(0x1fff);
    CHECK_EQ_INT(input.move_x, 0);
    CHECK_EQ_INT(input.move_y, 0);
}

TEST(update_reports_pressed_on_the_first_calc_down)
{
    BUTTONS buttons = {};
    TH06_INPUT input = {};
    input.shoot = true;
    Buttons_Update(&buttons, &input);
    CHECK(buttons.held.shoot);
    CHECK(buttons.pressed.shoot);
    CHECK(buttons.repeated.shoot);
    CHECK_EQ_INT(buttons.held_frames, 0);

    Buttons_Update(&buttons, &input);
    CHECK(buttons.held.shoot);
    CHECK(!buttons.pressed.shoot);
    CHECK_EQ_INT(buttons.held_frames, 1);
}

TEST(update_releases_and_presses_again)
{
    BUTTONS buttons = {};
    TH06_INPUT down = {};
    down.shoot = true;
    const TH06_INPUT up = {};
    Buttons_Update(&buttons, &down);
    Buttons_Update(&buttons, &up);
    CHECK(!buttons.held.shoot);
    CHECK(!buttons.pressed.shoot);
    Buttons_Update(&buttons, &down);
    CHECK(buttons.pressed.shoot);
}

TEST(update_repeats_every_8_calcs_once_held_30)
{
    BUTTONS buttons = {};
    TH06_INPUT input = {};
    input.shoot = true;
    for (int calc = 0; calc <= 73; calc++) {
        Buttons_Update(&buttons, &input);
        const bool should_repeat = calc == 0 || calc == 33 || calc == 41
            || calc == 49 || calc == 57 || calc == 65 || calc == 73;
        CHECK_EQ_INT(buttons.repeated.shoot, should_repeat);
    }
    CHECK_EQ_INT(buttons.held_frames, 33);
}

TEST(update_caps_held_frames_at_30_once_repeating)
{
    BUTTONS buttons = {};
    TH06_INPUT input = {};
    input.shoot = true;
    for (int calc = 0; calc < 60; calc++) {
        Buttons_Update(&buttons, &input);
    }
    CHECK(buttons.held_frames >= 30);
    CHECK(buttons.held_frames <= 38);
}

TEST(clear_keeps_held_frames_and_counts_as_pressed_next)
{
    BUTTONS buttons = {};
    TH06_INPUT input = {};
    input.shoot = true;
    for (int calc = 0; calc < 5; calc++) {
        Buttons_Update(&buttons, &input);
    }
    const int32_t held_frames = buttons.held_frames;
    Buttons_Clear(&buttons);
    CHECK_EQ_INT(buttons.held_frames, held_frames);
    CHECK(!buttons.held.shoot);
    CHECK_EQ_INT(buttons.word, 0);

    Buttons_Update(&buttons, &input);
    CHECK(buttons.pressed.shoot);
}

TEST(replay_uses_the_saved_previous_word_after_live_input)
{
    BUTTONS buttons = { .word = 1, .held_frames = 31 };
    const TH06_INPUT live = { .menu = true };
    Buttons_Update(&buttons, &live);
    CHECK_EQ_INT(buttons.previous_word, 1);
    CHECK_EQ_INT(buttons.held_frames, 0);
    Buttons_ApplyReplay(&buttons, 1);
    CHECK_EQ_INT(buttons.previous_word, 1);
    CHECK_EQ_INT(buttons.word, 9);
    CHECK(buttons.pressed.menu);
    CHECK(!buttons.pressed.shoot);
}

TEST(replay_updates_the_repeat_counter_against_the_same_snapshot)
{
    BUTTONS buttons = { .word = 1, .held_frames = 31 };
    const TH06_INPUT input = { .shoot = true };
    Buttons_Update(&buttons, &input);
    CHECK_EQ_INT(buttons.held_frames, 32);
    CHECK(!buttons.repeated.shoot);
    Buttons_ApplyReplay(&buttons, 1);
    CHECK_EQ_INT(buttons.held_frames, 33);
    CHECK(buttons.repeated.shoot);
    CHECK(!buttons.pressed.shoot);
}

TEST(masked_press_counts_partial_release_while_another_bit_stays_down)
{
    BUTTONS buttons = { .word = 3 };
    const TH06_INPUT input = { .shoot = true };
    Buttons_Update(&buttons, &input);
    CHECK(Buttons_WasPressed(&buttons, 3));
    CHECK(!Buttons_WasPressed(&buttons, 1));
    CHECK(!Buttons_WasPressed(&buttons, 2));
    CHECK(!buttons.pressed.shoot);
    Buttons_Clear(&buttons);
    CHECK_EQ_INT(buttons.previous_word, 0);
}

TEST(pressed_or_repeated_counts_a_partial_release)
{
    BUTTONS buttons = { .word = 0x1001 };
    const TH06_INPUT input = { .confirm = true };
    Buttons_Update(&buttons, &input);
    CHECK(Buttons_WasPressedOrRepeated(&buttons, 0x1001));
    CHECK(!Buttons_WasPressedOrRepeated(&buttons, 0x1000));
}

TEST(pressed_or_repeated_counts_a_held_bit_on_a_repeat)
{
    BUTTONS buttons = {};
    const TH06_INPUT input = { .up = true };
    int32_t count = 0;
    for (int calc = 0; calc <= 33; calc++) {
        Buttons_Update(&buttons, &input);
        if (Buttons_WasPressedOrRepeated(&buttons, 0x10)) {
            count++;
        }
        CHECK(!Buttons_WasPressedOrRepeated(&buttons, 0x20));
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

static void M_HoldPadShoot(BUTTONS *const buttons, const bool held)
{
    M_SetDevice(INPUT_DEVICE_GAMEPAD, (TH06_INPUT) { .shoot = held });
    const TH06_INPUT input = { .shoot = held, .focus = held };
    Buttons_Update(buttons, &input);
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
    BUTTONS buttons = { .shared_focus = true };
    for (int calc = 1; calc <= 7; calc++) {
        M_HoldPadShoot(&buttons, true);
        CHECK(buttons.held.shoot);
        CHECK(!buttons.held.focus);
    }
    M_HoldPadShoot(&buttons, true);
    CHECK(buttons.held.focus);
}

TEST(shared_pad_shoot_counts_up_to_16)
{
    M_Reset();
    BUTTONS buttons = { .shared_focus = true };
    for (int calc = 0; calc < 20; calc++) {
        M_HoldPadShoot(&buttons, true);
    }
    CHECK_EQ_INT(buttons.shot_held_frames, 16);
}

TEST(shared_pad_release_above_8_takes_8_off_without_focus)
{
    M_Reset();
    BUTTONS buttons = { .shared_focus = true, .shot_held_frames = 16 };
    M_HoldPadShoot(&buttons, false);
    CHECK(!buttons.held.focus);
    CHECK_EQ_INT(buttons.shot_held_frames, 8);
    M_HoldPadShoot(&buttons, false);
    CHECK_EQ_INT(buttons.shot_held_frames, 0);
}

TEST(shared_pad_quick_repress_focuses_at_once)
{
    M_Reset();
    BUTTONS buttons = { .shared_focus = true, .shot_held_frames = 16 };
    M_HoldPadShoot(&buttons, false);
    M_HoldPadShoot(&buttons, true);
    CHECK(buttons.held.focus);
    CHECK_EQ_INT(buttons.shot_held_frames, 9);
}

TEST(shared_joystick_shoot_counts_like_the_gamepad)
{
    M_Reset();
    BUTTONS buttons = { .shared_focus = true, .shot_held_frames = 7 };
    M_SetDevice(INPUT_DEVICE_JOYSTICK, (TH06_INPUT) { .shoot = true });
    const TH06_INPUT input = { .shoot = true, .focus = true };
    Buttons_Update(&buttons, &input);
    CHECK(buttons.held.focus);
}

TEST(shared_keyboard_focus_is_immediate)
{
    M_Reset();
    BUTTONS buttons = { .shared_focus = true };
    M_SetDevice(INPUT_DEVICE_KEYBOARD, (TH06_INPUT) { .focus = true });
    const TH06_INPUT input = { .focus = true };
    Buttons_Update(&buttons, &input);
    CHECK(buttons.held.focus);
}

TEST(unshared_focus_is_the_inputs)
{
    M_Reset();
    BUTTONS buttons = {};
    M_HoldPadShoot(&buttons, true);
    CHECK(buttons.held.focus);
    CHECK_EQ_INT(buttons.shot_held_frames, 0);
}

TEST(read_shared_focus_finds_a_pad_binding_of_shoot_and_focus)
{
    FakeShell_Reset();
    M_Bind(0, "z");
    M_Bind(0, "joy_button_3");
    M_Bind(2, "z");
    BUTTONS buttons = { .shared_focus = true };
    Buttons_ReadSharedFocus(&buttons);
    CHECK(!buttons.shared_focus);
    M_Bind(2, "joy_button_3");
    Buttons_ReadSharedFocus(&buttons);
    CHECK(buttons.shared_focus);
    FakeShell_Reset();
}
