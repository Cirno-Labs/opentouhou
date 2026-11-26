#include "pc98/pmd/part.h"

#include "test.h"

TEST(gate_wraps_before_minimum_and_slur_overrides_all_rules)
{
    const struct {
        PMD_GATE rules;
        uint8_t duration, expected;
        bool slur;
    } cases[] = {
        { { 200, 128, 0 }, 200, 44, false },
        { { 200, 128, 180 }, 200, 20, false },
        { { 200, 128, 180 }, 100, 0, false },
        { { 200, 128, 180 }, 200, 0, true },
        { { 200, 0, 0 }, 3, 200, false },
        { { 200, 0, 0 }, 0, 200, false },
    };
    for (size_t i = 0; i < ARRAY_SIZE(cases); i++) {
        PMD_PART part = {};
        PMD_CONTROL control = {};
        part.gate = cases[i].rules;
        CHECK(Result_Ignore(PmdPart_Note(
            &part, &control, false, 0x40, cases[i].duration, cases[i].slur,
            false, 0, 1)));
        CHECK_EQ_INT(part.note.gate, cases[i].expected);
    }
}

TEST(key_off_marks_due_once_and_arm_replaces_all_flag_bits)
{
    PMD_PART part = {};
    part.note.gate = 10;
    CHECK(!PmdPart_KeyOffDue(&part, 11));
    CHECK(PmdPart_KeyOffDue(&part, 10));
    CHECK_EQ_INT(part.note.key_flags, 255);
    CHECK(!PmdPart_KeyOffDue(&part, 0));
    PmdPart_Arm(&part, true);
    CHECK_EQ_INT(part.note.key_flags, 2);
    CHECK(!PmdPart_KeyOffDue(&part, 0));
    PmdPart_Arm(&part, false);
    CHECK_EQ_INT(part.note.key_flags, 0);
    part.note.key_flags = 0x80;
    CHECK(PmdPart_KeyOffDue(&part, 0));
    CHECK_EQ_INT(part.note.key_flags, 255);
}

TEST(notes_resolve_previous_and_ties_advance_without_reset)
{
    PMD_PART part = {};
    PMD_CONTROL control = {};
    part.pitch.glide = 77;
    part.lfos[0].wave = 2;
    const uint8_t parameters[] = { 0, 1, 1, 1 };
    PmdLfo_Configure(&part.lfos[0], parameters);
    part.lfos[1].value = 1234;
    part.lfo_flags = 1;
    CHECK(Result_Ignore(
        PmdPart_Note(&part, &control, false, 0x40, 8, false, false, 0, 1)));
    CHECK_EQ_INT(part.note.previous, 0x40);
    CHECK_EQ_INT(part.pitch.glide, 0);
    CHECK_EQ_INT(part.pitch.frequency, 0x226a);
    CHECK_EQ_INT(part.lfos[0].value, 1);
    CHECK_EQ_INT(part.lfos[1].value, 1234);
    control.tied = true;
    part.pitch.glide = 55;
    CHECK(Result_Ignore(
        PmdPart_Note(&part, &control, false, 0x4c, 8, false, false, 0, 1)));
    CHECK_EQ_INT(part.note.previous, 0x40);
    CHECK_EQ_INT(part.pitch.frequency, 0x226a);
    CHECK_EQ_INT(part.lfos[0].value, 0xffff);
    CHECK_EQ_INT(part.pitch.glide, 0);
}

TEST(rests_keep_glide_and_do_not_reset_active_lfo_delay)
{
    PMD_PART part = {};
    PMD_CONTROL control = {};
    part.pitch.glide = 42;
    part.pitch.frequency = 0x1234;
    part.lfos[0].value = 0x9999;
    part.lfos[0].delay = 3;
    part.lfo_flags = 1;
    CHECK(Result_Ignore(
        PmdPart_Note(&part, &control, false, 0x4f, 8, false, false, 0, 1)));
    CHECK_EQ_INT(part.note.previous, 0x4f);
    CHECK_EQ_INT(part.pitch.note, 255);
    CHECK_EQ_INT(part.lfos[0].delay, 2);
    CHECK_EQ_INT(part.lfos[0].value, 0x9999);
    CHECK_EQ_INT(part.pitch.frequency, 0x1234);
    CHECK_EQ_INT(part.pitch.glide, 42);
    CHECK(Result_Ignore(
        PmdPart_Note(&part, &control, false, 0x0c, 8, false, false, 0, 1)));
    CHECK_EQ_INT(part.note.previous, 0x4f);
    CHECK_EQ_INT(part.lfos[0].delay, 1);
    CHECK_EQ_INT(part.pitch.glide, 42);
}

TEST(primary_and_secondary_use_shared_rng_in_order)
{
    PMD_PART part = {};
    PMD_CONTROL control = { .random_seed = 14676 };
    part.lfos[0].wave = part.lfos[1].wave = 3;
    const uint8_t parameters[] = { 0, 1, 1, 1 };
    PmdLfo_Configure(&part.lfos[0], parameters);
    PmdLfo_Configure(&part.lfos[1], parameters);
    part.lfo_flags = 0x11;
    CHECK(Result_Ignore(
        PmdPart_Note(&part, &control, false, 0x40, 8, false, false, 0, 1)));
    CHECK_EQ_INT(part.note.previous, 0x40);
    CHECK_EQ_INT(part.lfos[0].value, 1);
    CHECK_EQ_INT(part.lfos[1].value, 0);
    CHECK_EQ_INT(control.random_seed, 32512);
}

TEST(async_and_timer_flags_select_independent_reset_and_step_rules)
{
    PMD_PART part = {};
    PMD_CONTROL control = {};
    part.lfos[0].speed = 1;
    part.lfos[0].delta = 7;
    part.lfos[0].period = 2;
    part.lfos[0].wave = 2;
    part.lfo_flags = 5;
    CHECK(Result_Ignore(
        PmdPart_Note(&part, &control, false, 0x40, 8, false, false, 0, 1)));
    CHECK_EQ_INT(part.lfos[0].value, 14);
    part.lfo_modes = 2;
    CHECK(Result_Ignore(
        PmdPart_Note(&part, &control, false, 0x41, 8, false, false, 0, 0)));
    CHECK_EQ_INT(part.lfos[0].value, 14);
    part.note.previous = 0x4c;
    part.lfo_flags = 0;
    CHECK(!Result_Ignore(
        PmdPart_Note(&part, &control, false, 0x2c, 8, false, false, 0, 0)));
    CHECK_EQ_INT(part.note.previous, 0x4c);
}
