#include "th02/ending/private_verdict.h"

#include "core/base/assert.h"

#include <string.h>

typedef struct {
    uint8_t row;
    int16_t left;
    uint16_t columns;
    uint16_t hold;
    bool draw;
} VERDICT_ROW_LAYOUT;

static const VERDICT_ROW_LAYOUT m_Rows[] = {
    { 0, 24, 21, 200, true }, { 1, 64, 16, 100, true },
    { 2, 64, 16, 100, true }, { 3, 64, 16, 100, true },
    { 4, 64, 16, 100, true }, { 5, 64, 16, 150, true },
    { 6, 64, 6, 120, true },  { 6, 192, 24, 200, false },
    { 8, 64, 18, 0, true },
};

static VERDICT m_Verdict;
VERDICT *const p_Verdict = &m_Verdict;

static void M_Delay(const uint16_t frames)
{
    Wait_Delay(&m_Verdict.wait, frames);
}

static void M_Fade(
    const PALETTE_FADE_KIND kind, const int16_t speed,
    const VERDICT_PHASE phase)
{
    Wait_Fade(&m_Verdict.wait, kind, speed);
    m_Verdict.phase = phase;
}

void Verdict_Init(void)
{
    memset(&m_Verdict, 0, sizeof(m_Verdict));
}

void Verdict_Begin(
    const ENDING_ASSETS *const assets, const TH02_SESSION *const session,
    const ENDING_STORY route, const uint16_t style)
{
    ASSERT(route >= ENDING_STORY_BAD && route < ENDING_STORY_COUNT);
    Verdict_Init();
    m_Verdict.assets = assets;
    m_Verdict.session = session;
    m_Verdict.route = route;
    m_Verdict.style = style;
}

bool Verdict_Update(const TH02_INPUT *const input)
{
    for (;;) {
        switch (m_Verdict.phase) {
        case VERDICT_PREPARE:
            EndingRender_Stage(
                &m_Verdict.assets->images, ENDING_IMAGE_VERDICT, true);
            Video_CopyPage(0);
            M_Fade(PALETTE_BLACK_IN, 4, VERDICT_FADE_IN);
            return false;

        case VERDICT_FADE_IN:
            if (!Wait_Ready(&m_Verdict.wait, nullptr, 0)) {
                return false;
            }
            M_Delay(100);
            m_Verdict.phase = VERDICT_ROW;
            return false;

        case VERDICT_ROW: {
            if (!Wait_Ready(&m_Verdict.wait, nullptr, 0)) {
                return false;
            }
            ASSERT(m_Verdict.row < ARRAY_SIZE(m_Rows));
            const VERDICT_ROW_LAYOUT *const row = &m_Rows[m_Verdict.row];
            if (row->draw) {
                Video_AccessPage(1);
                EndingRender_VerdictRow(
                    &m_Verdict.assets->labels,
                    &m_Verdict.assets->verdict[m_Verdict.route],
                    &m_Verdict.assets->gaiji, m_Verdict.session, row->row,
                    m_Verdict.style);
            }
            EndingRender_BeginReveal(
                &m_Verdict.reveal, &m_Verdict.assets->masks, row->left,
                (int16_t)(96 + row->row * 32), row->columns);
            m_Verdict.phase = VERDICT_REVEAL;
            break;
        }

        case VERDICT_REVEAL:
            if (!Wait_Ready(&m_Verdict.wait, nullptr, 0)) {
                return false;
            }
            if (!EndingRender_StepReveal(&m_Verdict.reveal)) {
                M_Delay(10);
                return false;
            }
            if (m_Rows[m_Verdict.row].hold != 0) {
                M_Delay(m_Rows[m_Verdict.row].hold);
                m_Verdict.phase = VERDICT_NEXT_ROW;
                return false;
            }
            Wait_Key(&m_Verdict.wait, input);
            m_Verdict.phase = VERDICT_CONFIRM;
            return false;

        case VERDICT_NEXT_ROW:
            if (!Wait_Ready(&m_Verdict.wait, nullptr, 0)) {
                return false;
            }
            m_Verdict.row++;
            m_Verdict.phase = VERDICT_ROW;
            break;

        case VERDICT_CONFIRM:
            if (!Wait_Ready(&m_Verdict.wait, input, 0)) {
                return false;
            }
            M_Fade(PALETTE_BLACK_OUT, 5, VERDICT_FADE_OUT);
            return false;

        case VERDICT_FADE_OUT:
            if (!Wait_Ready(&m_Verdict.wait, nullptr, 0)) {
                return false;
            }
            m_Verdict.phase = VERDICT_DONE;
            return true;

        case VERDICT_DONE:
            return true;
        }
    }
}
