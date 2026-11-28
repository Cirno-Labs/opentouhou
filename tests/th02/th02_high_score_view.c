#include "tests/th02/th02_high_score_view.h"

static const TEST_HIGH_SCORE_VIEW *m_HighScoreViewObserver;

static void M_HighScoreViewBegin(
    const HIGH_SCORES *const scores, const TH02_SETTINGS *const settings,
    const HIGH_SCORE_VIEW_ASSETS *const assets)
{
    (void)scores;
    (void)settings;
    (void)assets;
    if (m_HighScoreViewObserver != nullptr
        && m_HighScoreViewObserver->begin != nullptr) {
        m_HighScoreViewObserver->begin(m_HighScoreViewObserver->context);
    }
}

static void M_HighScoreViewFrame(
    const HIGH_SCORE_VIEW_ASSETS *const assets, const int16_t frame)
{
    (void)assets;
    if (m_HighScoreViewObserver != nullptr
        && m_HighScoreViewObserver->frame != nullptr) {
        m_HighScoreViewObserver->frame(m_HighScoreViewObserver->context, frame);
    }
}

static void M_HighScoreViewFlip(const uint8_t drawing, const uint8_t visible)
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

const HIGH_SCORE_VIEW_RENDER g_TestHighScoreViewRender = {
    .begin = M_HighScoreViewBegin,
    .frame = M_HighScoreViewFrame,
    .flip = M_HighScoreViewFlip,
    .end = M_HighScoreViewEnd,
};

void TestHighScoreView_Observe(const TEST_HIGH_SCORE_VIEW *view)
{
    m_HighScoreViewObserver = view;
}

void TestHighScoreView_Begin(
    const uint16_t duration, const TEST_HIGH_SCORE_VIEW *const view)
{
    static const HIGH_SCORE_VIEW_ASSETS assets;
    TestHighScoreView_Observe(view);
    HighScoreView_Init(&g_TestHighScoreViewRender);
    HighScoreView_Begin(nullptr, nullptr, &assets, duration);
}
