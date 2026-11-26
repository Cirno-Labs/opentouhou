#include "th02/score_view/logic.h"

#include "core/base/assert.h"

#include <string.h>

typedef enum {
    SCORE_VIEW_POLL,
    SCORE_VIEW_FLIP,
    SCORE_VIEW_EXIT,
    SCORE_VIEW_HOLD,
    SCORE_VIEW_DONE,
} SCORE_VIEW_PHASE;

typedef struct {
    const SCORE_VIEW_ASSETS *assets;
    SCORE_VIEW_PHASE phase;
    int16_t frame;
    uint16_t duration;
    uint8_t visible_page;
    bool input_allowed;
} SCORE_VIEWER;

static SCORE_VIEWER m_Viewer;

void ScoreView_Init(void)
{
    memset(&m_Viewer, 0, sizeof(m_Viewer));
}

void ScoreView_Begin(
    const SCORES *const scores, const TH02_SETTINGS *const settings,
    const SCORE_VIEW_ASSETS *const assets, const uint16_t duration)
{
    ASSERT(duration <= 32766);
    ScoreView_Init();
    m_Viewer.assets = assets;
    m_Viewer.duration = duration;
    m_Viewer.visible_page = 1;
    if (assets != nullptr) {
        ScoreViewRender_Begin(scores, settings, assets);
    }
}

SCORE_VIEW_EVENTS ScoreView_Update(const uint16_t keys)
{
    if (m_Viewer.phase == SCORE_VIEW_DONE) {
        return (SCORE_VIEW_EVENTS) { .done = true };
    }
    if (m_Viewer.phase == SCORE_VIEW_HOLD) {
        if (m_Viewer.assets != nullptr) {
            ScoreViewRender_End();
        }
        m_Viewer.phase = SCORE_VIEW_DONE;
        return (SCORE_VIEW_EVENTS) { .done = true };
    }
    if (m_Viewer.phase == SCORE_VIEW_FLIP) {
        const uint8_t drawing_page = m_Viewer.visible_page;
        m_Viewer.visible_page ^= 1;
        if (m_Viewer.assets != nullptr) {
            ScoreViewRender_Flip(drawing_page, m_Viewer.visible_page);
        }
        m_Viewer.phase = m_Viewer.frame > m_Viewer.duration ? SCORE_VIEW_EXIT
                                                            : SCORE_VIEW_POLL;
    }
    if (m_Viewer.phase == SCORE_VIEW_POLL) {
        if (!m_Viewer.input_allowed && keys == 0) {
            m_Viewer.input_allowed = true;
        } else if (m_Viewer.input_allowed && keys != 0) {
            m_Viewer.phase = SCORE_VIEW_EXIT;
        }
        if (m_Viewer.phase == SCORE_VIEW_POLL) {
            m_Viewer.frame++;
            if (m_Viewer.assets != nullptr) {
                ScoreViewRender_Frame(m_Viewer.assets, m_Viewer.frame);
            }
            m_Viewer.phase = SCORE_VIEW_FLIP;
            return (SCORE_VIEW_EVENTS) { .wait_frames = 1 };
        }
    }
    m_Viewer.phase = SCORE_VIEW_HOLD;
    return (SCORE_VIEW_EVENTS) { .wait_frames = 20 };
}
