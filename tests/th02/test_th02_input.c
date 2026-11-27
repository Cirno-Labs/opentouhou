#include "tests/test.h"
#include "th02/input.h"

#include <string.h>

TEST(sensing_keeps_contradictions_and_keypad_diagonal_flags)
{
    TH02_INPUT sample = { .up = true,
                          .down = true,
                          .left = true,
                          .right = true,
                          .up_left = true,
                          .down_right = true };
    CHECK_EQ_INT(GameInput_Sense(&sample), 0x900f);
    sample = (TH02_INPUT) { .space = true, .q = true, .cancel = true };
    CHECK_EQ_INT(GameInput_Sense(&sample), 0x0150);
    sample.shot = true;
    CHECK_EQ_INT(GameInput_Sense(&sample), 0x0150);
    CHECK_EQ_INT(GameInput_Sense(&(TH02_INPUT) {}), 0);
}

TEST(wait_keys_preserve_the_original_narrow_group_selection)
{
    TH02_INPUT sample = {
        .up = true, .down_left = true, .space = true, .q = true
    };
    CHECK(!GameInput_AnyWaitKey(&sample));
    sample.extra_group_3 = true;
    CHECK(GameInput_AnyWaitKey(&sample));
    CHECK_EQ_INT(GameInput_Sense(&sample), 0x4111);
    CHECK(GameInput_AnyWaitKey(&(TH02_INPUT) { .shot = true }));
    CHECK(GameInput_AnyWaitKey(&(TH02_INPUT) { .bomb = true }));
    CHECK(GameInput_AnyWaitKey(&(TH02_INPUT) { .ok = true }));
    CHECK(GameInput_AnyWaitKey(&(TH02_INPUT) { .cancel = true }));
    CHECK(GameInput_AnyWaitKey(&(TH02_INPUT) { .extra_group_0 = true }));
    CHECK(GameInput_AnyWaitKey(&(TH02_INPUT) { .extra_group_5 = true }));
}

TEST(wait_needs_two_clear_release_samples_before_a_new_press_pair)
{
    const TH02_INPUT held = { .shot = true };
    const TH02_INPUT clear = {};
    KEY_WAIT wait;
    GameInput_BeginWait(&wait, &held);
    CHECK(!GameInput_AdvanceWait(&wait, &clear));
    CHECK(!GameInput_AdvanceWait(&wait, &clear));
    CHECK_EQ_INT(wait.phase, KEY_WAIT_RELEASE);
    CHECK(!GameInput_AdvanceWait(&wait, &held));
    CHECK(!GameInput_AdvanceWait(&wait, &clear));
    CHECK_EQ_INT(wait.phase, KEY_WAIT_PRESS);
    CHECK(!GameInput_AdvanceWait(&wait, &clear));
    CHECK(GameInput_AdvanceWait(&wait, &held));
    CHECK_EQ_INT(wait.phase, KEY_WAIT_DONE);
    const KEY_WAIT completed = wait;
    CHECK(GameInput_AdvanceWait(&wait, &clear));
    CHECK(memcmp(&wait, &completed, sizeof(wait)) == 0);
}

TEST(wait_ignores_taps_between_pair_endpoints_and_space_during_wait)
{
    const TH02_INPUT clear = {}, held = { .ok = true },
                     space = { .space = true };
    KEY_WAIT wait;
    GameInput_BeginWait(&wait, &clear);
    CHECK(!GameInput_AdvanceWait(&wait, &held));
    CHECK(!GameInput_AdvanceWait(&wait, &clear));
    CHECK_EQ_INT(wait.phase, KEY_WAIT_PRESS);
    CHECK(!GameInput_AdvanceWait(&wait, &held));
    CHECK(!GameInput_AdvanceWait(&wait, &space));
    CHECK_EQ_INT(wait.phase, KEY_WAIT_PRESS);
    CHECK(!GameInput_AdvanceWait(&wait, &clear));
    CHECK(GameInput_AdvanceWait(&wait, &held));
}

TEST(logo_accepts_arrows_and_wait_groups_but_ignores_the_keypad)
{
    TH02_INPUT sample = { .keypad_up = true,
                          .keypad_down = true,
                          .keypad_left = true,
                          .keypad_right = true,
                          .up_left = true,
                          .down_right = true };
    CHECK_EQ_INT(GameInput_Sense(&sample), 0x900f);
    CHECK(!GameInput_LogoKey(&sample));
    CHECK(!GameInput_AnyWaitKey(&sample));
    sample.up = true;
    CHECK(GameInput_LogoKey(&sample));
    CHECK(GameInput_LogoKey(&(TH02_INPUT) { .q = true }));
    CHECK(GameInput_LogoKey(&(TH02_INPUT) { .space = true }));
    CHECK(GameInput_LogoKey(&(TH02_INPUT) { .extra_group_0 = true }));
    CHECK(GameInput_LogoKey(&(TH02_INPUT) { .extra_group_3 = true }));
    CHECK(GameInput_LogoKey(&(TH02_INPUT) { .extra_group_5 = true }));
    CHECK(!GameInput_LogoKey(&(TH02_INPUT) {}));
}
