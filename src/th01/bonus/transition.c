#include "th01/bonus/bonus.h"

#include "core/base/assert.h"

#include <string.h>

typedef struct {
    uint16_t transferred[VIDEO_HEIGHT];
    bool active[VIDEO_HEIGHT];
    uint16_t activated;
    uint16_t done;
    uint16_t previous_done;
    bool pending;
    bool waiting;
} BONUS_ROW_TRANSITION;

static BONUS_ROW_TRANSITION m_Transition;

void Bonus_RowShift32(const uint16_t y, const uint16_t transferred)
{
    ASSERT(y < VIDEO_HEIGHT && transferred <= 76 && transferred % 4 == 0);
    uint8_t *const row = g_Video.pages[0] + (size_t)y * VIDEO_WIDTH;
    const uint8_t *const source = g_Video.pages[1] + (size_t)y * VIDEO_WIDTH;
    if ((y & 1) != 0) {
        memmove(row + 32, row, VIDEO_WIDTH - 32);
        memcpy(row, source + VIDEO_WIDTH - 32 - transferred * 8, 32);
    } else {
        memmove(row, row + 32, VIDEO_WIDTH - 32);
        memcpy(row + VIDEO_WIDTH - 32, source + transferred * 8, 32);
    }
    Video_SelectDrawingPage(0);
}

void Bonus_BeginRowTransition(void)
{
    ASSERT(!m_Transition.pending);
    memset(&m_Transition, 0, sizeof(m_Transition));
    m_Transition.previous_done = 1;
    m_Transition.pending = true;
}

bool Bonus_ResumeRowTransition(const PALETTE *const target)
{
    while (m_Transition.pending) {
        if (!m_Transition.waiting) {
            for (uint16_t y = 0; y < VIDEO_HEIGHT; y++) {
                if (!m_Transition.active[y]) {
                    continue;
                }
                Bonus_RowShift32(y, m_Transition.transferred[y]);
                m_Transition.transferred[y] += 4;
                if (m_Transition.transferred[y] >= 80) {
                    m_Transition.active[y] = false;
                    m_Transition.done++;
                }
            }
            m_Transition.waiting = true;
        }
        if (!VSyncIrq_DelayReady(2)) {
            return false;
        }
        m_Transition.waiting = false;
        if (m_Transition.done >= VIDEO_HEIGHT) {
            m_Transition.pending = false;
            return true;
        }
        if (m_Transition.activated < VIDEO_HEIGHT) {
            for (uint16_t y = m_Transition.activated;
                 y < m_Transition.activated + 8; y++) {
                m_Transition.active[y] = true;
            }
        }
        m_Transition.activated += 8;
        if (m_Transition.done != m_Transition.previous_done
            && (m_Transition.activated & 15) == 0) {
            Palette_StepGlobalTo(target, false);
        }
        m_Transition.previous_done = m_Transition.done;
    }
    return true;
}

void Bonus_InitRowTransition(void)
{
    memset(&m_Transition, 0, sizeof(m_Transition));
}
