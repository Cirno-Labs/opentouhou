#include "tests/test.h"
#include "tests/th02/th02_shot_select.c"

static SHOT_SELECT_EVENTS M_Loop(const uint16_t keys, int *const frames)
{
    SHOT_SELECT_EVENTS events = ShotSelect_Update(keys);
    *frames = 0;
    while (events.wait == SHOT_SELECT_FRAME && *frames < 16) {
        (*frames)++;
        events = ShotSelect_Update((uint16_t)~keys);
    }
    return events;
}

TEST(shot_confirmation_is_immediate_and_completion_requests_are_not_repeated)
{
    TH02_SESSION session = { .shot_type = 1, .frame = 1234 };
    const TH02_SESSION before = session;
    TestShotSelect_Begin(&session, nullptr);
    CHECK(memcmp(&session, &before, sizeof(session)) == 0);
    CHECK_EQ_INT(m_Select.selection, 1);
    const SHOT_SELECT_EVENTS events = ShotSelect_Update(GAME_KEY_SHOT);
    CHECK_EQ_INT(events.wait, SHOT_SELECT_FADE_OUT);
    CHECK_EQ_INT(events.release_images, 7);
    CHECK_EQ_INT(session.shot_type, 1);
    CHECK_EQ_INT(session.frame, 1234);
    const SHOT_SELECT_EVENTS done = ShotSelect_Update(GAME_KEY_RIGHT);
    CHECK_EQ_INT(done.wait, SHOT_SELECT_COMPLETE);
    CHECK_EQ_INT(done.release_images, 0);
}

TEST(shot_both_directions_keep_different_draw_orders_and_one_input_sample)
{
    TH02_SESSION session = { .shot_type = 1 };
    TEST_SHOT_TRACE trace = {};
    TestShotSelect_Begin(&session, &trace);
    int frames;
    const SHOT_SELECT_EVENTS events =
        M_Loop(GAME_KEY_LEFT | GAME_KEY_RIGHT | GAME_KEY_OK, &frames);
    CHECK_EQ_INT(events.wait, SHOT_SELECT_FADE_OUT);
    CHECK_EQ_INT(frames, 12);
    CHECK_EQ_INT(trace.count, 14);
    CHECK_EQ_INT(session.shot_type, 1);
    CHECK_EQ_INT(trace.requests[0].kind, SHOT_SELECT_DESCRIPTION);
    CHECK_EQ_INT(trace.requests[0].value, 7);
    CHECK_EQ_INT(trace.requests[1].kind, SHOT_SELECT_BACKGROUND);
    CHECK_EQ_INT(trace.requests[2].kind, SHOT_SELECT_PICTURE);
    CHECK_EQ_INT(trace.requests[2].left, 232);
    CHECK_EQ_INT(trace.requests[2].top, 232);
    CHECK_EQ_INT(trace.requests[6].kind, SHOT_SELECT_PICTURE);
    CHECK_EQ_INT(trace.requests[6].selection, 0);
    CHECK_EQ_INT(trace.requests[6].left, 16);
    CHECK_EQ_INT(trace.requests[7].kind, SHOT_SELECT_BACKGROUND);
    CHECK_EQ_INT(trace.requests[8].kind, SHOT_SELECT_PICTURE);
    CHECK_EQ_INT(trace.requests[9].kind, SHOT_SELECT_DESCRIPTION);
    CHECK_EQ_INT(trace.requests[13].kind, SHOT_SELECT_PICTURE);
    CHECK_EQ_INT(trace.requests[13].selection, 1);
    CHECK_EQ_INT(trace.requests[13].left, 224);
}

TEST(shot_held_direction_repeats_after_thirty_loop_waits)
{
    TH02_SESSION session = { .shot_type = 1 };
    TestShotSelect_Begin(&session, nullptr);
    int frames;
    for (int i = 0; i < 32; i++) {
        const SHOT_SELECT_EVENTS events = M_Loop(GAME_KEY_RIGHT, &frames);
        CHECK_EQ_INT(events.wait, SHOT_SELECT_INPUT);
        CHECK_EQ_INT(frames, i == 0 || i == 31 ? 7 : 1);
    }
    CHECK_EQ_INT(m_Select.selection, 0);
    CHECK_EQ_INT(m_Select.input_delay, 32);
    CHECK_EQ_INT(m_Select.input_locked, 0);
    CHECK_EQ_INT(session.shot_type, 1);
    CHECK_EQ_INT(M_Loop(0, &frames).wait, SHOT_SELECT_INPUT);
    CHECK_EQ_INT(m_Select.input_delay, 0);
    CHECK_EQ_INT(ShotSelect_Update(GAME_KEY_OK).wait, SHOT_SELECT_FADE_OUT);
    CHECK_EQ_INT(session.shot_type, 0);
}

TEST(shot_cancel_locks_input_and_repeat_counter_wrap_preserves_that_lock)
{
    TH02_SESSION session = { .shot_type = 1, .frame = -1 };
    TestShotSelect_Begin(&session, nullptr);
    m_Select.input_locked = 1;
    m_Select.input_delay = UINT16_MAX;
    int frames;
    CHECK_EQ_INT(M_Loop(GAME_KEY_CANCEL, &frames).wait, SHOT_SELECT_INPUT);
    CHECK_EQ_INT(frames, 1);
    CHECK_EQ_INT(m_Select.input_delay, 0);
    CHECK_EQ_INT(m_Select.input_locked, GAME_KEY_CANCEL);
    CHECK_EQ_INT(M_Loop(GAME_KEY_SHOT, &frames).wait, SHOT_SELECT_INPUT);
    CHECK_EQ_INT(m_Select.input_delay, 1);
    CHECK_EQ_INT(M_Loop(0, &frames).wait, SHOT_SELECT_INPUT);
    CHECK_EQ_INT(m_Select.input_locked, 0);
    CHECK_EQ_INT(ShotSelect_Update(GAME_KEY_OK).wait, SHOT_SELECT_FADE_OUT);
    CHECK_EQ_INT(session.frame, -1);
}
