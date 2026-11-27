#include "tests/th02/th02_high_score_view.h"

static const TEST_HIGH_SCORE_VIEW *m_HighScoreViewObserver;

static void M_HighScoreViewBegin(
    const HIGH_SCORES *scores, const TH02_SETTINGS *settings,
    const HIGH_SCORE_VIEW_ASSETS *assets)
{
    if (m_HighScoreViewObserver != nullptr
        && m_HighScoreViewObserver->begin != nullptr) {
        m_HighScoreViewObserver->begin(m_HighScoreViewObserver->context);
    }
}

static void M_HighScoreViewFrame(
    const HIGH_SCORE_VIEW_ASSETS *assets, int16_t frame)
{
    if (m_HighScoreViewObserver != nullptr
        && m_HighScoreViewObserver->frame != nullptr) {
        m_HighScoreViewObserver->frame(m_HighScoreViewObserver->context, frame);
    }
}

static void M_HighScoreViewFlip(uint8_t drawing, uint8_t visible)
{
    if (m_HighScoreViewObserver != nullptr
        && m_HighScoreViewObserver->flip != nullptr) {
        m_HighScoreViewObserver->flip(
            m_HighScoreViewObserver->context, drawing, visible);
    }
}

static void M_HighScoreViewEnd(void)
{
    if (m_HighScoreViewObserver != nullptr
        && m_HighScoreViewObserver->end != nullptr) {
        m_HighScoreViewObserver->end(m_HighScoreViewObserver->context);
    }
}

#define HighScoreViewRender_Begin M_HighScoreViewBegin
#define HighScoreViewRender_Frame M_HighScoreViewFrame
#define HighScoreViewRender_Flip M_HighScoreViewFlip
#define HighScoreViewRender_End M_HighScoreViewEnd
#include "th02/high_score_view/logic.c"
#undef HighScoreViewRender_Begin
#undef HighScoreViewRender_Frame
#undef HighScoreViewRender_Flip
#undef HighScoreViewRender_End

void TestHighScoreView_Observe(const TEST_HIGH_SCORE_VIEW *view)
{
    m_HighScoreViewObserver = view;
}

void TestHighScoreView_Begin(
    uint16_t duration, const TEST_HIGH_SCORE_VIEW *view)
{
    static const HIGH_SCORE_VIEW_ASSETS assets;
    TestHighScoreView_Observe(view);
    HighScoreView_Begin(nullptr, nullptr, &assets, duration);
}
