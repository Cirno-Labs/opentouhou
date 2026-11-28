#include "tests/test.h"
#include "tests/th02/th02_pause_menu.h"
#include "th02/pause_menu/private.h"

static PAUSE_MENU_RESULT M_Poll(const uint16_t keys)
{
    CHECK_EQ_INT(PauseMenu_Update(UINT16_MAX).result, PAUSE_MENU_INPUT);
    return PauseMenu_Update(keys).result;
}

TEST(persistent_selection_enters_confirmation_and_yes_precedes_cancel)
{
    TEST_PAUSE_REQUESTS requests = {};
    TestPause_Observe(&requests);
    TestPause_Init();
    const PAUSE_MENU_ASSETS assets = {};
    p_Pause->keys = GAME_KEY_CANCEL;
    p_Pause->selected = 1;
    p_Pause->first_color = 0xaa;
    p_Pause->second_color = 0xbb;
    PauseMenu_Begin(&assets, p_Pause->keys);
    CHECK_EQ_INT(p_Pause->selected, 1);
    CHECK_EQ_INT(p_Pause->first_color, 0xaa);
    CHECK_EQ_INT(p_Pause->second_color, 0xbb);
    CHECK_EQ_INT(p_Pause->keys, GAME_KEY_CANCEL);
    CHECK_EQ_INT(requests.draws[0].kind, PAUSE_MENU_RESUME_TEXT);
    CHECK_EQ_INT(requests.draws[0].attribute, 0xe9);
    CHECK_EQ_INT(requests.tone, 70);
    CHECK_EQ_INT(M_Poll(0), PAUSE_MENU_FRAME);
    CHECK_EQ_INT(M_Poll(GAME_KEY_OK), PAUSE_MENU_FRAME);
    CHECK_EQ_INT(p_Pause->state, 3);
    CHECK_EQ_INT(p_Pause->selected, 0);
    CHECK_EQ_INT(M_Poll(0), PAUSE_MENU_FRAME);
    CHECK_EQ_INT(
        M_Poll(GAME_KEY_UP | GAME_KEY_DOWN | GAME_KEY_OK | GAME_KEY_CANCEL),
        PAUSE_MENU_QUIT);
    CHECK_EQ_INT(p_Pause->selected, 1);
    CHECK_EQ_INT(requests.tone, 70);
    const size_t count = requests.count;
    const uint16_t frame = p_Pause->frame;
    CHECK_EQ_INT(PauseMenu_Update(0).result, PAUSE_MENU_QUIT);
    CHECK_EQ_INT(requests.count, count);
    CHECK_EQ_INT(p_Pause->frame, frame);
}

TEST(no_redraws_choices_before_cancel_and_resume_waits_for_release)
{
    TEST_PAUSE_REQUESTS requests = {};
    TestPause_Observe(&requests);
    TestPause_Init();
    const PAUSE_MENU_ASSETS assets = {};
    p_Pause->selected = 1;
    PauseMenu_Begin(&assets, p_Pause->keys);
    CHECK_EQ_INT(M_Poll(0), PAUSE_MENU_FRAME);
    CHECK_EQ_INT(M_Poll(GAME_KEY_SHOT), PAUSE_MENU_FRAME);
    CHECK_EQ_INT(M_Poll(0), PAUSE_MENU_FRAME);
    requests.count = 0;
    CHECK_EQ_INT(M_Poll(GAME_KEY_OK | GAME_KEY_CANCEL), PAUSE_MENU_FRAME);
    CHECK_EQ_INT(p_Pause->state, 2);
    CHECK_EQ_INT(requests.count, 6);
    CHECK_EQ_INT(requests.draws[1].kind, PAUSE_MENU_BLANK);
    CHECK_EQ_INT(requests.draws[4].kind, PAUSE_MENU_RESUME_TEXT);
    CHECK_EQ_INT(requests.draws[5].kind, PAUSE_MENU_QUIT_TEXT);
    CHECK_EQ_INT(M_Poll(GAME_KEY_CANCEL), PAUSE_MENU_FRAME);
    CHECK_EQ_INT(requests.tone, 70);
    requests.count = 0;
    CHECK_EQ_INT(M_Poll(0), PAUSE_MENU_RESUME);
    CHECK_EQ_INT(requests.tone, 100);
    CHECK_EQ_INT(p_Pause->keys, 0);
    CHECK_EQ_INT(requests.count, 5);
    CHECK_EQ_INT(requests.draws[1].x, 18);
    CHECK_EQ_INT(requests.draws[1].y, 12);
    CHECK_EQ_INT(requests.draws[4].x, 17);
    CHECK_EQ_INT(requests.draws[4].y, 16);
}

TEST(heading_blinks_before_input_and_frame_wrap_keeps_release_gating)
{
    TEST_PAUSE_REQUESTS requests = {};
    TestPause_Observe(&requests);
    TestPause_Init();
    const PAUSE_MENU_ASSETS assets = {};
    PauseMenu_Begin(&assets, p_Pause->keys);
    p_Pause->frame = UINT16_MAX;
    CHECK_EQ_INT(M_Poll(GAME_KEY_LEFT), PAUSE_MENU_FRAME);
    CHECK_EQ_INT(requests.draws[2].kind, PAUSE_MENU_BLANK);
    CHECK_EQ_INT(p_Pause->frame, 0);
    CHECK_EQ_INT(p_Pause->state, 0);
    CHECK_EQ_INT(M_Poll(GAME_KEY_OK), PAUSE_MENU_FRAME);
    CHECK_EQ_INT(requests.draws[3].kind, PAUSE_MENU_HEADING);
    CHECK_EQ_INT(p_Pause->state, 0);
    CHECK_EQ_INT(M_Poll(0), PAUSE_MENU_FRAME);
    CHECK_EQ_INT(p_Pause->state, 1);
    CHECK_EQ_INT(M_Poll(GAME_KEY_CANCEL), PAUSE_MENU_FRAME);
    CHECK_EQ_INT(p_Pause->state, 2);
    CHECK_EQ_INT(M_Poll(0), PAUSE_MENU_RESUME);
}

TEST(pause_returns_retained_keys_until_the_input_sample_and_after_completion)
{
    TestPause_Observe(nullptr);
    TestPause_Init();
    PauseMenu_Begin(nullptr, GAME_KEY_CANCEL);
    PAUSE_MENU_EVENT event = PauseMenu_Update(GAME_KEY_UP);
    CHECK_EQ_INT(event.result, PAUSE_MENU_INPUT);
    CHECK_EQ_INT(event.keys, GAME_KEY_CANCEL);
    event = PauseMenu_Update(GAME_KEY_LEFT);
    CHECK_EQ_INT(event.result, PAUSE_MENU_FRAME);
    CHECK_EQ_INT(event.keys, GAME_KEY_LEFT);
    event = PauseMenu_Update(0);
    CHECK_EQ_INT(event.result, PAUSE_MENU_INPUT);
    CHECK_EQ_INT(event.keys, GAME_KEY_LEFT);
    event = PauseMenu_Update(0);
    CHECK_EQ_INT(event.result, PAUSE_MENU_FRAME);
    CHECK_EQ_INT(event.keys, 0);
    CHECK_EQ_INT(PauseMenu_Update(0).result, PAUSE_MENU_INPUT);
    event = PauseMenu_Update(GAME_KEY_CANCEL);
    CHECK_EQ_INT(event.result, PAUSE_MENU_FRAME);
    CHECK_EQ_INT(event.keys, GAME_KEY_CANCEL);
    CHECK_EQ_INT(PauseMenu_Update(0).result, PAUSE_MENU_INPUT);
    event = PauseMenu_Update(0);
    CHECK_EQ_INT(event.result, PAUSE_MENU_RESUME);
    CHECK_EQ_INT(event.keys, 0);
    event = PauseMenu_Update(GAME_KEY_UP);
    CHECK_EQ_INT(event.result, PAUSE_MENU_RESUME);
    CHECK_EQ_INT(event.keys, 0);
}
