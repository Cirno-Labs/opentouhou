#include "th07/buttons.h"

#include "test.h"
#include "shell_fake.h"

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
        { offsetof(TH07_INPUT, shoot), 0x1 },
        { offsetof(TH07_INPUT, bomb), 0x2 },
        { offsetof(TH07_INPUT, focus), 0x4 },
        { offsetof(TH07_INPUT, menu), 0x8 },
        { offsetof(TH07_INPUT, up), 0x10 },
        { offsetof(TH07_INPUT, down), 0x20 },
        { offsetof(TH07_INPUT, left), 0x40 },
        { offsetof(TH07_INPUT, right), 0x80 },
        { offsetof(TH07_INPUT, skip), 0x100 },
        { offsetof(TH07_INPUT, quit), 0x200 },
        { offsetof(TH07_INPUT, unlock_end), 0x400 },
        { offsetof(TH07_INPUT, unlock_start), 0x800 },
        { offsetof(TH07_INPUT, confirm), 0x1000 },
        { offsetof(TH07_INPUT, cheat), 0x2000 },
        { offsetof(TH07_INPUT, retry), 0x4000 },
    };
    for (size_t i = 0; i < ARRAY_SIZE(cases); i++) {
        TH07_INPUT input = {};
        *(bool *)((uint8_t *)&input + cases[i].offset) = true;
        const uint16_t word = Buttons_ToWord(&input);
        CHECK_EQ_INT(word, cases[i].bit);

        const TH07_INPUT back = Buttons_FromWord(word);
        CHECK(*(const bool *)((const uint8_t *)&back + cases[i].offset));
        CHECK_EQ_INT(Buttons_ToWord(&back), cases[i].bit);
    }
}

// The INI keys follow this order.
TEST(schema_keeps_the_action_order)
{
    CHECK_EQ_INT(g_InputSchema.count, 16);
    CHECK_EQ_INT(TH07_ACTION_shoot, 0);
    CHECK_EQ_INT(TH07_ACTION_focus, 2);
    CHECK_EQ_INT(TH07_ACTION_confirm, 12);
    CHECK_EQ_INT(TH07_ACTION_cheat, 13);
    CHECK_EQ_INT(TH07_ACTION_retry, 14);
    CHECK_EQ_INT(TH07_ACTION_move, 15);
    CHECK_EQ_STR(g_InputSchema.actions[0].name, "shoot");
    CHECK_EQ_STR(g_InputSchema.actions[2].name, "focus");
    CHECK_EQ_STR(g_InputSchema.actions[12].name, "confirm");
    CHECK_EQ_STR(g_InputSchema.actions[13].name, "cheat");
    CHECK_EQ_STR(g_InputSchema.actions[14].name, "retry");
    CHECK_EQ_STR(g_InputSchema.actions[15].name, "move");
    CHECK_EQ_STR(
        g_InputSchema.actions[13].defaults[INPUT_DEVICE_JOYSTICK],
        "joy_button_7");
    CHECK_EQ_STR(
        g_InputSchema.actions[2].defaults[INPUT_DEVICE_KEYBOARD],
        "left_shift, right_shift");
}

TEST(from_word_leaves_the_stick_at_zero)
{
    const TH07_INPUT input = Buttons_FromWord(0x7fff);
    CHECK_EQ_INT(input.move_x, 0);
    CHECK_EQ_INT(input.move_y, 0);
}

TEST(update_reports_pressed_on_the_first_calc_down)
{
    BUTTONS buttons = {};
    const TH07_INPUT input = { .shoot = true };
    Buttons_Update(&buttons, &input, false);
    CHECK(buttons.held.shoot);
    CHECK(buttons.pressed.shoot);
    CHECK(buttons.repeated.shoot);
    CHECK_EQ_INT(buttons.held_frames, 0);

    Buttons_Update(&buttons, &input, false);
    CHECK(buttons.held.shoot);
    CHECK(!buttons.pressed.shoot);
    CHECK_EQ_INT(buttons.held_frames, 1);
}

TEST(update_repeats_every_8_calcs_once_held_30)
{
    BUTTONS buttons = {};
    const TH07_INPUT input = { .shoot = true };
    for (int calc = 0; calc <= 73; calc++) {
        Buttons_Update(&buttons, &input, false);
        const bool should_repeat = calc == 0 || calc == 33 || calc == 41
            || calc == 49 || calc == 57 || calc == 65 || calc == 73;
        CHECK_EQ_INT(buttons.repeated.shoot, should_repeat);
    }
    CHECK_EQ_INT(buttons.held_frames, 33);
}

TEST(clear_keeps_held_frames_and_counts_as_pressed_next)
{
    BUTTONS buttons = {};
    const TH07_INPUT input = { .shoot = true };
    for (int calc = 0; calc < 5; calc++) {
        Buttons_Update(&buttons, &input, false);
    }
    const int32_t held_frames = buttons.held_frames;
    Buttons_Clear(&buttons);
    CHECK_EQ_INT(buttons.held_frames, held_frames);
    CHECK(!buttons.held.shoot);
    CHECK_EQ_INT(buttons.word, 0);

    Buttons_Update(&buttons, &input, false);
    CHECK(buttons.pressed.shoot);
}

TEST(pressed_or_repeated_counts_a_held_bit_on_a_repeat)
{
    BUTTONS buttons = {};
    const TH07_INPUT input = { .up = true };
    int32_t count = 0;
    for (int calc = 0; calc <= 33; calc++) {
        Buttons_Update(&buttons, &input, false);
        if (Buttons_WasPressedOrRepeated(&buttons, 0x10)) {
            count++;
        }
        CHECK(!Buttons_WasPressedOrRepeated(&buttons, 0x20));
    }
    CHECK_EQ_INT(count, 2);
}

TEST(merge_adds_bits_and_keeps_the_previous_word_and_repeat)
{
    BUTTONS buttons = {};
    const TH07_INPUT shoot = { .shoot = true };
    for (int calc = 0; calc <= 33; calc++) {
        Buttons_Update(&buttons, &shoot, false);
    }
    CHECK(buttons.repeat);
    const int32_t held_frames = buttons.held_frames;

    const TH07_INPUT bomb = { .bomb = true };
    Buttons_Merge(&buttons, &bomb, false);
    CHECK_EQ_INT(buttons.word, 0x3);
    CHECK_EQ_INT(buttons.previous_word, 0x1);
    CHECK_EQ_INT(buttons.held_frames, held_frames);
    CHECK(buttons.repeat);
    CHECK(buttons.held.shoot);
    CHECK(buttons.pressed.bomb);
    CHECK(!buttons.pressed.shoot);
    CHECK(buttons.repeated.shoot);
}

TEST(merge_keeps_what_was_pressed_on_the_dropped_calc)
{
    BUTTONS buttons = {};
    const TH07_INPUT shoot = { .shoot = true };
    Buttons_Update(&buttons, &shoot, false);
    Buttons_Merge(&buttons, &shoot, false);
    CHECK_EQ_INT(buttons.previous_word, 0);
    CHECK(buttons.pressed.shoot);
    CHECK(Buttons_WasPressed(&buttons, 0x1));
}

static const TH07_INPUT m_Shoot = { .shoot = true };
static const TH07_INPUT m_NoInput;

// Holds or releases the shoot of the device kind, and of the input that
// the shell merges from every device.
static const TH07_INPUT *M_PadShoot(const INPUT_DEVICE device, const bool held)
{
    FakeShell_Reset();
    for (int32_t other = 0; other < INPUT_DEVICE_COUNT; other++) {
        FakeShell_SetDeviceInput(
            (INPUT_DEVICE)other, &m_NoInput, sizeof(m_NoInput));
    }
    const TH07_INPUT *const input = held ? &m_Shoot : &m_NoInput;
    FakeShell_SetDeviceInput(device, input, sizeof(*input));
    return input;
}

static void M_HoldPadShoot(BUTTONS *const buttons, const int32_t calcs)
{
    for (int32_t i = 0; i < calcs; i++) {
        Buttons_Update(buttons, M_PadShoot(INPUT_DEVICE_GAMEPAD, true), true);
    }
}

TEST(pad_shoot_focuses_from_the_10th_held_calc)
{
    BUTTONS buttons = {};
    for (int32_t calc = 1; calc <= 12; calc++) {
        M_HoldPadShoot(&buttons, 1);
        CHECK_EQ_INT(buttons.held.focus, calc >= 10);
        CHECK(buttons.held.shoot);
    }
    CHECK_EQ_INT(buttons.shot_held_frames, 12);
}

TEST(pad_shoot_count_stops_at_20)
{
    BUTTONS buttons = {};
    M_HoldPadShoot(&buttons, 25);
    CHECK_EQ_INT(buttons.shot_held_frames, 20);
}

TEST(release_above_10_takes_10_off_and_focuses_that_calc)
{
    BUTTONS buttons = {};
    M_HoldPadShoot(&buttons, 20);
    const TH07_INPUT *const released = M_PadShoot(INPUT_DEVICE_GAMEPAD, false);
    Buttons_Update(&buttons, released, true);
    CHECK_EQ_INT(buttons.shot_held_frames, 10);
    CHECK(buttons.held.focus);
    CHECK(!buttons.held.shoot);

    Buttons_Update(&buttons, released, true);
    CHECK_EQ_INT(buttons.shot_held_frames, 0);
    CHECK(!buttons.held.focus);
}

TEST(release_at_10_or_below_resets_without_focus)
{
    BUTTONS buttons = {};
    M_HoldPadShoot(&buttons, 10);
    CHECK(buttons.held.focus);
    Buttons_Update(&buttons, M_PadShoot(INPUT_DEVICE_GAMEPAD, false), true);
    CHECK_EQ_INT(buttons.shot_held_frames, 0);
    CHECK(!buttons.held.focus);
}

TEST(joystick_shoot_counts_like_the_gamepad)
{
    BUTTONS buttons = {};
    for (int32_t calc = 1; calc <= 15; calc++) {
        Buttons_Update(&buttons, M_PadShoot(INPUT_DEVICE_JOYSTICK, true), true);
        CHECK_EQ_INT(buttons.held.focus, calc >= 10);
    }
    Buttons_Update(&buttons, M_PadShoot(INPUT_DEVICE_JOYSTICK, false), true);
    CHECK_EQ_INT(buttons.shot_held_frames, 5);
    CHECK(buttons.held.focus);
}

TEST(keyboard_shoot_never_focuses)
{
    BUTTONS buttons = {};
    for (int32_t calc = 0; calc < 30; calc++) {
        Buttons_Update(&buttons, M_PadShoot(INPUT_DEVICE_KEYBOARD, true), true);
        CHECK(!buttons.held.focus);
    }
    CHECK_EQ_INT(buttons.shot_held_frames, 0);
}

TEST(focus_button_focuses_at_once_while_pad_shoot_is_held)
{
    BUTTONS buttons = {};
    M_PadShoot(INPUT_DEVICE_GAMEPAD, true);
    const TH07_INPUT input = { .shoot = true, .focus = true };
    Buttons_Update(&buttons, &input, true);
    CHECK(buttons.held.focus);
    CHECK_EQ_INT(buttons.shot_held_frames, 1);
}

TEST(setting_off_gives_the_input_focus_and_no_count)
{
    BUTTONS buttons = {};
    for (int32_t calc = 0; calc < 15; calc++) {
        Buttons_Update(&buttons, M_PadShoot(INPUT_DEVICE_GAMEPAD, true), false);
        CHECK(!buttons.held.focus);
    }
    CHECK_EQ_INT(buttons.shot_held_frames, 0);

    const TH07_INPUT focus = { .focus = true };
    Buttons_Update(&buttons, &focus, false);
    CHECK(buttons.held.focus);
}

TEST(merge_steps_the_shoot_count)
{
    BUTTONS buttons = {};
    M_HoldPadShoot(&buttons, 9);
    CHECK(!buttons.held.focus);
    Buttons_Merge(&buttons, M_PadShoot(INPUT_DEVICE_GAMEPAD, true), true);
    CHECK_EQ_INT(buttons.shot_held_frames, 10);
    CHECK(buttons.held.focus);
}

TEST(clear_keeps_the_shoot_count)
{
    BUTTONS buttons = {};
    M_HoldPadShoot(&buttons, 15);
    Buttons_Clear(&buttons);
    CHECK_EQ_INT(buttons.shot_held_frames, 15);
    M_HoldPadShoot(&buttons, 1);
    CHECK(buttons.held.focus);
}
