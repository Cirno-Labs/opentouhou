#include "th07/buttons.h"

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

TEST(update_repeats_every_8_calcs_once_held_30)
{
    BUTTONS buttons = {};
    const TH07_INPUT input = { .shoot = true };
    for (int calc = 0; calc <= 73; calc++) {
        Buttons_Update(&buttons, &input);
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

TEST(pressed_or_repeated_counts_a_held_bit_on_a_repeat)
{
    BUTTONS buttons = {};
    const TH07_INPUT input = { .up = true };
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

TEST(merge_adds_bits_and_keeps_the_previous_word_and_repeat)
{
    BUTTONS buttons = {};
    const TH07_INPUT shoot = { .shoot = true };
    for (int calc = 0; calc <= 33; calc++) {
        Buttons_Update(&buttons, &shoot);
    }
    CHECK(buttons.repeat);
    const int32_t held_frames = buttons.held_frames;

    const TH07_INPUT bomb = { .bomb = true };
    Buttons_Merge(&buttons, &bomb);
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
    Buttons_Update(&buttons, &shoot);
    Buttons_Merge(&buttons, &shoot);
    CHECK_EQ_INT(buttons.previous_word, 0);
    CHECK(buttons.pressed.shoot);
    CHECK(Buttons_WasPressed(&buttons, 0x1));
}
