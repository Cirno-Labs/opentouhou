#include "tests/test.h"
#include "tests/th02/th02_menu.h"
#include "th02/main_menu/private.h"

TEST(main_menu_requires_release_and_handles_up_then_down_independently)
{
    TH02_SESSION session = {};
    TEST_MENU_TRACE trace = {};
    TestMenu_Begin(&session, &trace, false, 1);
    MAIN_MENU_EVENTS events = MainMenu_Update(GAME_KEY_SHOT);
    CHECK_EQ_INT(events.action, MAIN_MENU_NONE);
    CHECK(events.wait_frame && !p_MainMenu->input_allowed);
    CHECK_EQ_INT(trace.init, 1);
    CHECK_EQ_INT(trace.count, 6);
    CHECK_EQ_INT(p_MainMenu->idle_frame, 1);
    MainMenu_Update(GAME_KEY_SHOT);
    CHECK_EQ_INT(p_MainMenu->idle_frame, 2);
    MainMenu_Update(0);
    CHECK(p_MainMenu->input_allowed);
    MainMenu_Update(GAME_KEY_DOWN);
    CHECK_EQ_INT(p_MainMenu->nav.selection, 2);
    CHECK_EQ_INT(p_MainMenu->idle_frame, 1);
    MainMenu_Update(GAME_KEY_DOWN);
    CHECK_EQ_INT(p_MainMenu->nav.selection, 2);
    MainMenu_Update(0);
    trace.count = 0;
    events = MainMenu_Update(GAME_KEY_UP | GAME_KEY_DOWN);
    CHECK_EQ_INT(events.action, MAIN_MENU_NONE);
    CHECK_EQ_INT(p_MainMenu->nav.selection, 2);
    CHECK_EQ_INT(trace.count, 4);
    CHECK_EQ_INT(trace.selections[0], 2);
    CHECK_EQ_INT(trace.selections[1], 0);
    CHECK_EQ_INT(trace.selections[2], 0);
    CHECK_EQ_INT(trace.selections[3], 2);
    CHECK_EQ_INT(trace.attributes[0], 0xc1);
    CHECK_EQ_INT(trace.attributes[1], 0xe1);
    CHECK_EQ_INT(trace.attributes[2], 0xc1);
    CHECK_EQ_INT(trace.attributes[3], 0xe1);
}

TEST(unreleased_keys_allow_idle_demo_and_demo_return_only_finishes_the_frame)
{
    TH02_SESSION session = { .frame = INT32_MAX };
    TestMenu_Begin(&session, nullptr, true, 3);
    for (size_t i = 0; i < 641; i++) {
        const MAIN_MENU_EVENTS events = MainMenu_Update(GAME_KEY_Q);
        CHECK_EQ_INT(events.action, MAIN_MENU_NONE);
        CHECK(events.wait_frame && !events.quit);
    }
    MAIN_MENU_EVENTS events = MainMenu_Update(GAME_KEY_Q);
    CHECK_EQ_INT(events.action, MAIN_MENU_DEMO);
    CHECK_EQ_INT(events.demo, 3);
    CHECK(!events.wait_frame && !events.quit);
    CHECK_EQ_INT(p_MainMenu->idle_frame, 641);
    CHECK_EQ_INT(session.frame, (int32_t)((uint32_t)INT32_MAX + 641u));
    const int32_t before = session.frame;
    events = MainMenu_Update(GAME_KEY_CANCEL);
    CHECK_EQ_INT(events.action, MAIN_MENU_NONE);
    CHECK(!events.wait_frame);
    CHECK_EQ_INT(session.frame, before);
    events = MainMenu_Resume(GAME_KEY_CANCEL);
    CHECK(events.wait_frame && !events.quit);
    CHECK_EQ_INT(p_MainMenu->idle_frame, 642);
    CHECK_EQ_INT(session.frame, before + 1);
    events = MainMenu_Resume(GAME_KEY_CANCEL);
    CHECK(!events.wait_frame && !events.quit);
}

TEST(score_return_uses_live_keys_and_can_immediately_request_a_demo)
{
    TH02_SESSION session = {};
    TEST_MENU_TRACE trace = {};
    TestMenu_Begin(&session, &trace, true, 2);
    MainMenu_Update(0);
    p_MainMenu->nav.selection = 2;
    p_MainMenu->idle_frame = 650;
    MAIN_MENU_EVENTS events = MainMenu_Update(GAME_KEY_SHOT);
    CHECK_EQ_INT(events.action, MAIN_MENU_SCORES);
    CHECK_EQ_INT(events.score_frames, 2000);
    CHECK_EQ_INT(trace.clear, 1);
    CHECK(!events.wait_frame);
    CHECK_EQ_INT(session.frame, 1);
    events = MainMenu_Resume(0);
    CHECK_EQ_INT(trace.restore, 1);
    CHECK(!p_MainMenu->initialized);
    CHECK_EQ_INT(events.action, MAIN_MENU_DEMO);
    CHECK_EQ_INT(events.demo, 2);
    CHECK_EQ_INT(session.frame, 1);
    events = MainMenu_Resume(GAME_KEY_CANCEL);
    CHECK(events.wait_frame && !events.quit);
    CHECK_EQ_INT(session.frame, 2);
    CHECK_EQ_INT(p_MainMenu->idle_frame, 651);
    events = MainMenu_Update(GAME_KEY_CANCEL);
    CHECK(events.wait_frame && !events.quit);
    CHECK(!p_MainMenu->input_allowed);
    CHECK_EQ_INT(p_MainMenu->idle_frame, 1);
}

TEST(option_selection_and_cancel_still_complete_one_frame_before_quit)
{
    TH02_SESSION session = { .frame = -1 };
    TestMenu_Begin(&session, nullptr, true, 1);
    MainMenu_Update(0);
    p_MainMenu->nav.selection = 3;
    const MAIN_MENU_EVENTS events =
        MainMenu_Update(GAME_KEY_SHOT | GAME_KEY_CANCEL);
    CHECK_EQ_INT(events.action, MAIN_MENU_OPTIONS);
    CHECK(events.wait_frame && events.quit);
    CHECK(p_MainMenu->nav.in_option && !p_MainMenu->initialized);
    CHECK_EQ_INT(p_MainMenu->nav.selection, 0);
    CHECK_EQ_INT(p_MainMenu->idle_frame, 1);
    CHECK_EQ_INT(session.frame, 1);
}

TEST(shared_navigation_skips_extra_only_outside_options)
{
    MENU_NAV nav = { .selection = 0 };
    Menu_Move(&nav, 5, 1);
    CHECK_EQ_INT(nav.selection, 2);
    Menu_Move(&nav, 5, -1);
    CHECK_EQ_INT(nav.selection, 0);
    Menu_Move(&nav, 5, -1);
    CHECK_EQ_INT(nav.selection, 5);
    nav.selection = 0;
    nav.in_option = true;
    Menu_Move(&nav, 6, 1);
    CHECK_EQ_INT(nav.selection, 1);
    nav.in_option = false;
    nav.extra_unlocked = true;
    Menu_Move(&nav, 5, -1);
    CHECK_EQ_INT(nav.selection, 0);
}
