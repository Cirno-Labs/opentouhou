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
    memset(&g_Buttons, 0, sizeof(g_Buttons));
    const TH07_INPUT input = { .shoot = true };
    Buttons_Update(&input, false);
    CHECK((g_Buttons.word & TH07_BUTTON_shoot) != 0);
    CHECK(Buttons_WasPressed(TH07_BUTTON_shoot));
    CHECK(Buttons_WasPressedOrRepeated(TH07_BUTTON_shoot));
    CHECK_EQ_INT(g_Buttons.held_frames, 0);

    Buttons_Update(&input, false);
    CHECK((g_Buttons.word & TH07_BUTTON_shoot) != 0);
    CHECK(!Buttons_WasPressed(TH07_BUTTON_shoot));
    CHECK_EQ_INT(g_Buttons.held_frames, 1);
}

TEST(update_repeats_every_8_calcs_once_held_30)
{
    memset(&g_Buttons, 0, sizeof(g_Buttons));
    const TH07_INPUT input = { .shoot = true };
    for (int calc = 0; calc <= 73; calc++) {
        Buttons_Update(&input, false);
        const bool should_repeat = calc == 0 || calc == 33 || calc == 41
            || calc == 49 || calc == 57 || calc == 65 || calc == 73;
        CHECK_EQ_INT(
            Buttons_WasPressedOrRepeated(TH07_BUTTON_shoot), should_repeat);
    }
    CHECK_EQ_INT(g_Buttons.held_frames, 33);
}

TEST(clear_keeps_held_frames_and_counts_as_pressed_next)
{
    memset(&g_Buttons, 0, sizeof(g_Buttons));
    const TH07_INPUT input = { .shoot = true };
    for (int calc = 0; calc < 5; calc++) {
        Buttons_Update(&input, false);
    }
    const int32_t held_frames = g_Buttons.held_frames;
    Buttons_Clear();
    CHECK_EQ_INT(g_Buttons.held_frames, held_frames);
    CHECK((g_Buttons.word & TH07_BUTTON_shoot) == 0);
    CHECK_EQ_INT(g_Buttons.word, 0);

    Buttons_Update(&input, false);
    CHECK(Buttons_WasPressed(TH07_BUTTON_shoot));
}

TEST(pressed_or_repeated_counts_a_held_bit_on_a_repeat)
{
    memset(&g_Buttons, 0, sizeof(g_Buttons));
    const TH07_INPUT input = { .up = true };
    int32_t count = 0;
    for (int calc = 0; calc <= 33; calc++) {
        Buttons_Update(&input, false);
        if (Buttons_WasPressedOrRepeated(0x10)) {
            count++;
        }
        CHECK(!Buttons_WasPressedOrRepeated(0x20));
    }
    CHECK_EQ_INT(count, 2);
}

TEST(play_sets_the_play_word_and_leaves_the_live_word)
{
    memset(&g_Buttons, 0, sizeof(g_Buttons));
    g_Buttons.word = 0x1008;
    Buttons_Play(0x11);
    Buttons_Play(0x13);
    CHECK_EQ_INT(g_Buttons.play_word, 0x13);
    CHECK_EQ_INT(g_Buttons.previous_play_word, 0x11);
    CHECK_EQ_INT(g_Buttons.word, 0x1008);
    CHECK_EQ_INT(g_Buttons.held_frames, 0);
}

TEST(play_repeats_every_8_calcs_once_the_play_word_holds_30)
{
    memset(&g_Buttons, 0, sizeof(g_Buttons));
    g_Buttons.play_word = 0x1;
    for (int calc = 0; calc <= 73; calc++) {
        Buttons_Play(0x1);
        const bool should_repeat = calc == 32 || calc == 40 || calc == 48
            || calc == 56 || calc == 64 || calc == 72;
        CHECK_EQ_INT(g_Buttons.repeat, should_repeat);
    }
    CHECK_EQ_INT(g_Buttons.held_frames, 34);
    Buttons_Play(0x2);
    CHECK(!g_Buttons.repeat);
    CHECK_EQ_INT(g_Buttons.held_frames, 0);
}

TEST(merge_adds_bits_and_keeps_the_previous_word_and_repeat)
{
    memset(&g_Buttons, 0, sizeof(g_Buttons));
    const TH07_INPUT shoot = { .shoot = true };
    for (int calc = 0; calc <= 33; calc++) {
        Buttons_Update(&shoot, false);
    }
    CHECK(g_Buttons.repeat);
    const int32_t held_frames = g_Buttons.held_frames;

    const TH07_INPUT bomb = { .bomb = true };
    Buttons_Merge(&bomb, false);
    CHECK_EQ_INT(g_Buttons.word, 0x3);
    CHECK_EQ_INT(g_Buttons.previous_word, 0x1);
    CHECK_EQ_INT(g_Buttons.held_frames, held_frames);
    CHECK(g_Buttons.repeat);
    CHECK((g_Buttons.word & TH07_BUTTON_shoot) != 0);
    CHECK(Buttons_WasPressed(TH07_BUTTON_bomb));
    CHECK(!Buttons_WasPressed(TH07_BUTTON_shoot));
    CHECK(Buttons_WasPressedOrRepeated(TH07_BUTTON_shoot));
}

TEST(merge_keeps_what_was_pressed_on_the_dropped_calc)
{
    memset(&g_Buttons, 0, sizeof(g_Buttons));
    const TH07_INPUT shoot = { .shoot = true };
    Buttons_Update(&shoot, false);
    Buttons_Merge(&shoot, false);
    CHECK_EQ_INT(g_Buttons.previous_word, 0);
    CHECK(Buttons_WasPressed(TH07_BUTTON_shoot));
    CHECK(Buttons_WasPressed(0x1));
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

static void M_HoldPadShoot(const int32_t calcs)
{
    for (int32_t i = 0; i < calcs; i++) {
        Buttons_Update(M_PadShoot(INPUT_DEVICE_GAMEPAD, true), true);
    }
}

TEST(pad_shoot_focuses_from_the_10th_held_calc)
{
    memset(&g_Buttons, 0, sizeof(g_Buttons));
    for (int32_t calc = 1; calc <= 12; calc++) {
        M_HoldPadShoot(1);
        CHECK_EQ_INT((g_Buttons.word & TH07_BUTTON_focus) != 0, calc >= 10);
        CHECK((g_Buttons.word & TH07_BUTTON_shoot) != 0);
    }
    CHECK_EQ_INT(g_Buttons.shot_held_frames, 12);
}

TEST(pad_shoot_count_stops_at_20)
{
    memset(&g_Buttons, 0, sizeof(g_Buttons));
    M_HoldPadShoot(25);
    CHECK_EQ_INT(g_Buttons.shot_held_frames, 20);
}

TEST(release_above_10_takes_10_off_and_focuses_that_calc)
{
    memset(&g_Buttons, 0, sizeof(g_Buttons));
    M_HoldPadShoot(20);
    const TH07_INPUT *const released = M_PadShoot(INPUT_DEVICE_GAMEPAD, false);
    Buttons_Update(released, true);
    CHECK_EQ_INT(g_Buttons.shot_held_frames, 10);
    CHECK((g_Buttons.word & TH07_BUTTON_focus) != 0);
    CHECK((g_Buttons.word & TH07_BUTTON_shoot) == 0);

    Buttons_Update(released, true);
    CHECK_EQ_INT(g_Buttons.shot_held_frames, 0);
    CHECK((g_Buttons.word & TH07_BUTTON_focus) == 0);
}

TEST(release_at_10_or_below_resets_without_focus)
{
    memset(&g_Buttons, 0, sizeof(g_Buttons));
    M_HoldPadShoot(10);
    CHECK((g_Buttons.word & TH07_BUTTON_focus) != 0);
    Buttons_Update(M_PadShoot(INPUT_DEVICE_GAMEPAD, false), true);
    CHECK_EQ_INT(g_Buttons.shot_held_frames, 0);
    CHECK((g_Buttons.word & TH07_BUTTON_focus) == 0);
}

TEST(joystick_shoot_counts_like_the_gamepad)
{
    memset(&g_Buttons, 0, sizeof(g_Buttons));
    for (int32_t calc = 1; calc <= 15; calc++) {
        Buttons_Update(M_PadShoot(INPUT_DEVICE_JOYSTICK, true), true);
        CHECK_EQ_INT((g_Buttons.word & TH07_BUTTON_focus) != 0, calc >= 10);
    }
    Buttons_Update(M_PadShoot(INPUT_DEVICE_JOYSTICK, false), true);
    CHECK_EQ_INT(g_Buttons.shot_held_frames, 5);
    CHECK((g_Buttons.word & TH07_BUTTON_focus) != 0);
}

TEST(keyboard_shoot_never_focuses)
{
    memset(&g_Buttons, 0, sizeof(g_Buttons));
    for (int32_t calc = 0; calc < 30; calc++) {
        Buttons_Update(M_PadShoot(INPUT_DEVICE_KEYBOARD, true), true);
        CHECK((g_Buttons.word & TH07_BUTTON_focus) == 0);
    }
    CHECK_EQ_INT(g_Buttons.shot_held_frames, 0);
}

TEST(focus_button_focuses_at_once_while_pad_shoot_is_held)
{
    memset(&g_Buttons, 0, sizeof(g_Buttons));
    M_PadShoot(INPUT_DEVICE_GAMEPAD, true);
    const TH07_INPUT input = { .shoot = true, .focus = true };
    Buttons_Update(&input, true);
    CHECK((g_Buttons.word & TH07_BUTTON_focus) != 0);
    CHECK_EQ_INT(g_Buttons.shot_held_frames, 1);
}

TEST(setting_off_gives_the_input_focus_and_no_count)
{
    memset(&g_Buttons, 0, sizeof(g_Buttons));
    for (int32_t calc = 0; calc < 15; calc++) {
        Buttons_Update(M_PadShoot(INPUT_DEVICE_GAMEPAD, true), false);
        CHECK((g_Buttons.word & TH07_BUTTON_focus) == 0);
    }
    CHECK_EQ_INT(g_Buttons.shot_held_frames, 0);

    const TH07_INPUT focus = { .focus = true };
    Buttons_Update(&focus, false);
    CHECK((g_Buttons.word & TH07_BUTTON_focus) != 0);
}

TEST(merge_steps_the_shoot_count)
{
    memset(&g_Buttons, 0, sizeof(g_Buttons));
    M_HoldPadShoot(9);
    CHECK((g_Buttons.word & TH07_BUTTON_focus) == 0);
    Buttons_Merge(M_PadShoot(INPUT_DEVICE_GAMEPAD, true), true);
    CHECK_EQ_INT(g_Buttons.shot_held_frames, 10);
    CHECK((g_Buttons.word & TH07_BUTTON_focus) != 0);
}

TEST(clear_keeps_the_shoot_count)
{
    memset(&g_Buttons, 0, sizeof(g_Buttons));
    M_HoldPadShoot(15);
    Buttons_Clear();
    CHECK_EQ_INT(g_Buttons.shot_held_frames, 15);
    M_HoldPadShoot(1);
    CHECK((g_Buttons.word & TH07_BUTTON_focus) != 0);
}
