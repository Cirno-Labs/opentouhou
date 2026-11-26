#include "th02_score_view.c"

#include "core/base/utils.h"

#include "test.h"

typedef struct {
    int begin_count;
    int end_count;
    int render_count;
    int flip_count;
    int16_t frame;
    uint8_t drawing_page;
    uint8_t visible_page;
} TRACE;

static void M_Begin(void *const context)
{
    TRACE *const trace = context;
    trace->begin_count++;
    trace->drawing_page = 0;
    trace->visible_page = 1;
}

static void M_Render(void *const context, const int16_t frame)
{
    TRACE *const trace = context;
    trace->render_count++;
    trace->frame = frame;
}

static void M_Flip(
    void *const context, const uint8_t drawing_page, const uint8_t visible_page)
{
    TRACE *const trace = context;
    trace->flip_count++;
    trace->drawing_page = drawing_page;
    trace->visible_page = visible_page;
}

static void M_End(void *const context)
{
    TRACE *const trace = context;
    trace->end_count++;
}

static TEST_SCORE_VIEW M_Draw(TRACE *const trace)
{
    return (TEST_SCORE_VIEW) { M_Begin, M_Render, M_Flip, M_End, trace };
}

TEST(opening_key_must_be_released_before_a_later_press_exits)
{
    TRACE trace = {};
    const TEST_SCORE_VIEW draw = M_Draw(&trace);
    TestScoreView_Begin(2000, &draw);
    CHECK_EQ_INT(trace.begin_count, 1);
    CHECK_EQ_INT(trace.render_count, 0);
    for (int frame = 1; frame <= 3; frame++) {
        CHECK_EQ_INT(ScoreView_Update(0x80).wait_frames, 1);
        CHECK_EQ_INT(trace.frame, frame);
        CHECK(!m_Viewer.input_allowed);
    }
    CHECK_EQ_INT(ScoreView_Update(0).wait_frames, 1);
    CHECK(m_Viewer.input_allowed);
    CHECK_EQ_INT(trace.render_count, 4);
    const SCORE_VIEW_EVENTS exit = ScoreView_Update(0x1);
    CHECK_EQ_INT(exit.wait_frames, 20);
    CHECK(!exit.done);
    CHECK_EQ_INT(trace.render_count, 4);
    CHECK_EQ_INT(trace.flip_count, 4);
    CHECK_EQ_INT(trace.end_count, 0);
    CHECK(ScoreView_Update(0xffff).done);
    CHECK_EQ_INT(trace.end_count, 1);
    const SCORE_VIEWER completed = m_Viewer;
    CHECK(ScoreView_Update(0).done);
    CHECK(memcmp(&m_Viewer, &completed, sizeof(completed)) == 0);
    CHECK_EQ_INT(trace.end_count, 1);
}

TEST(timeout_flips_duration_plus_one_frames_before_the_final_hold)
{
    const uint16_t durations[] = { 0, 1, 2, 350, 2000, 32766 };
    for (size_t i = 0; i < ARRAY_SIZE(durations); i++) {
        TRACE trace = {};
        const TEST_SCORE_VIEW draw = M_Draw(&trace);
        TestScoreView_Begin(durations[i], &draw);
        for (uint16_t frame = 1; frame <= durations[i] + 1; frame++) {
            CHECK_EQ_INT(ScoreView_Update(0).wait_frames, 1);
            CHECK_EQ_INT(trace.frame, frame);
            CHECK_EQ_INT(trace.render_count, frame);
            CHECK_EQ_INT(trace.flip_count, frame - 1);
            CHECK_EQ_INT(trace.visible_page, frame % 2);
            CHECK_EQ_INT(trace.drawing_page, 1 - frame % 2);
        }
        CHECK_EQ_INT(ScoreView_Update(0).wait_frames, 20);
        CHECK_EQ_INT(trace.flip_count, durations[i] + 1);
        CHECK_EQ_INT(trace.visible_page, durations[i] % 2);
        CHECK(ScoreView_Update(0).done);
        CHECK_EQ_INT(trace.end_count, 1);
    }
}

TEST(any_nonzero_gameplay_mask_exits_after_a_clear_poll)
{
    for (uint16_t bit = 0; bit < 16; bit++) {
        ScoreView_Begin(nullptr, nullptr, nullptr, 2000);
        CHECK_EQ_INT(ScoreView_Update(0).wait_frames, 1);
        CHECK_EQ_INT(ScoreView_Update((uint16_t)(1u << bit)).wait_frames, 20);
        CHECK_EQ_INT(m_Viewer.frame, 1);
        CHECK(ScoreView_Update(0xffff).done);
    }
}
