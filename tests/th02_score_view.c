#include "th02_score_view.h"

static const TEST_SCORE_VIEW *m_ScoreViewObserver;

static void M_ScoreViewBegin(
    const SCORES *scores, const TH02_SETTINGS *settings,
    const SCORE_VIEW_ASSETS *assets)
{
    if (m_ScoreViewObserver != nullptr
        && m_ScoreViewObserver->begin != nullptr) {
        m_ScoreViewObserver->begin(m_ScoreViewObserver->context);
    }
}

static void M_ScoreViewFrame(const SCORE_VIEW_ASSETS *assets, int16_t frame)
{
    if (m_ScoreViewObserver != nullptr
        && m_ScoreViewObserver->frame != nullptr) {
        m_ScoreViewObserver->frame(m_ScoreViewObserver->context, frame);
    }
}

static void M_ScoreViewFlip(uint8_t drawing, uint8_t visible)
{
    if (m_ScoreViewObserver != nullptr
        && m_ScoreViewObserver->flip != nullptr) {
        m_ScoreViewObserver->flip(
            m_ScoreViewObserver->context, drawing, visible);
    }
}

static void M_ScoreViewEnd(void)
{
    if (m_ScoreViewObserver != nullptr && m_ScoreViewObserver->end != nullptr) {
        m_ScoreViewObserver->end(m_ScoreViewObserver->context);
    }
}

#define ScoreViewRender_Begin M_ScoreViewBegin
#define ScoreViewRender_Frame M_ScoreViewFrame
#define ScoreViewRender_Flip M_ScoreViewFlip
#define ScoreViewRender_End M_ScoreViewEnd
#include "th02/score_view/logic.c"
#undef ScoreViewRender_Begin
#undef ScoreViewRender_Frame
#undef ScoreViewRender_Flip
#undef ScoreViewRender_End

void TestScoreView_Observe(const TEST_SCORE_VIEW *view)
{
    m_ScoreViewObserver = view;
}

void TestScoreView_Begin(uint16_t duration, const TEST_SCORE_VIEW *view)
{
    static const SCORE_VIEW_ASSETS assets;
    TestScoreView_Observe(view);
    ScoreView_Begin(nullptr, nullptr, &assets, duration);
}
