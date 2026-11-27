#include "th02/high_score_view/logic.h"

#include "core/base/assert.h"

#include <string.h>

typedef enum {
    HIGH_SCORE_VIEW_POLL,
    HIGH_SCORE_VIEW_FLIP,
    HIGH_SCORE_VIEW_EXIT,
    HIGH_SCORE_VIEW_HOLD,
    HIGH_SCORE_VIEW_DONE,
} HIGH_SCORE_VIEW_PHASE;

typedef struct {
    const HIGH_SCORE_VIEW_ASSETS *assets;
    HIGH_SCORE_VIEW_PHASE phase;
    int16_t frame;
    uint16_t duration;
    uint8_t visible_page;
    bool input_allowed;
} HIGH_SCORE_VIEWER;

static HIGH_SCORE_VIEWER m_Viewer;

void HighScoreView_Init(void)
{
    memset(&m_Viewer, 0, sizeof(m_Viewer));
}

void HighScoreView_Begin(
    const HIGH_SCORES *const scores, const TH02_SETTINGS *const settings,
    const HIGH_SCORE_VIEW_ASSETS *const assets, const uint16_t duration)
{
    ASSERT(duration <= 32766);
    HighScoreView_Init();
    m_Viewer.assets = assets;
    m_Viewer.duration = duration;
    m_Viewer.visible_page = 1;
    if (assets != nullptr) {
        HighScoreViewRender_Begin(scores, settings, assets);
    }
}

HIGH_SCORE_VIEW_EVENTS HighScoreView_Update(const uint16_t keys)
{
    if (m_Viewer.phase == HIGH_SCORE_VIEW_DONE) {
        return (HIGH_SCORE_VIEW_EVENTS) { .done = true };
    }
    if (m_Viewer.phase == HIGH_SCORE_VIEW_HOLD) {
        if (m_Viewer.assets != nullptr) {
            HighScoreViewRender_End();
        }
        m_Viewer.phase = HIGH_SCORE_VIEW_DONE;
        return (HIGH_SCORE_VIEW_EVENTS) { .done = true };
    }
    if (m_Viewer.phase == HIGH_SCORE_VIEW_FLIP) {
        const uint8_t drawing_page = m_Viewer.visible_page;
        m_Viewer.visible_page ^= 1;
        if (m_Viewer.assets != nullptr) {
            HighScoreViewRender_Flip(drawing_page, m_Viewer.visible_page);
        }
        m_Viewer.phase = m_Viewer.frame > m_Viewer.duration
            ? HIGH_SCORE_VIEW_EXIT
            : HIGH_SCORE_VIEW_POLL;
    }
    if (m_Viewer.phase == HIGH_SCORE_VIEW_POLL) {
        if (!m_Viewer.input_allowed && keys == 0) {
            m_Viewer.input_allowed = true;
        } else if (m_Viewer.input_allowed && keys != 0) {
            m_Viewer.phase = HIGH_SCORE_VIEW_EXIT;
        }
        if (m_Viewer.phase == HIGH_SCORE_VIEW_POLL) {
            m_Viewer.frame++;
            if (m_Viewer.assets != nullptr) {
                HighScoreViewRender_Frame(m_Viewer.assets, m_Viewer.frame);
            }
            m_Viewer.phase = HIGH_SCORE_VIEW_FLIP;
            return (HIGH_SCORE_VIEW_EVENTS) { .wait_frames = 1 };
        }
    }
    m_Viewer.phase = HIGH_SCORE_VIEW_HOLD;
    return (HIGH_SCORE_VIEW_EVENTS) { .wait_frames = 20 };
}
