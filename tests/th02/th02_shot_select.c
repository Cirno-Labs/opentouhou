#include "tests/th02/th02_shot_select.h"

#include "th02/shot_select/private.h"

static TEST_SHOT_TRACE *m_Trace;

static void M_Record(
    const TEST_SHOT_DRAW_KIND kind, const int8_t selection, const int16_t left,
    const int16_t top, const uint8_t value)
{
    if (m_Trace->count < 32) {
        m_Trace->requests[m_Trace->count++] = (TEST_SHOT_DRAW) {
            kind, selection, left, top, value,
        };
    }
}

static void M_BeginDraw(
    TH02_SESSION *const session, const SHOT_SELECT_ASSETS *const assets,
    const bool cleared[3])
{
    (void)session;
    (void)assets;
    (void)cleared;
}

static void M_DescriptionDraw(
    const SHOT_SELECT_ASSETS *const assets, const uint8_t selection,
    const uint8_t color)
{
    (void)assets;
    if (m_Trace != nullptr) {
        M_Record(SHOT_SELECT_DESCRIPTION, selection, 0, 0, color);
    }
}

static void M_BackgroundDraw(const uint8_t selection, const bool highlight)
{
    if (m_Trace != nullptr) {
        M_Record(SHOT_SELECT_BACKGROUND, selection, 0, 0, highlight);
    }
}

static void M_PictureDraw(
    const SHOT_SELECT_ASSETS *const assets, const uint8_t selection,
    const int16_t left, const int16_t top)
{
    (void)assets;
    if (m_Trace != nullptr) {
        M_Record(SHOT_SELECT_PICTURE, selection, left, top, 0);
    }
}

static void M_DarkenDraw(const int16_t left, const int16_t top)
{
    if (m_Trace != nullptr) {
        M_Record(SHOT_SELECT_DARKEN, p_Select->selection, left, top, 0);
    }
}

const SHOT_SELECT_RENDER g_TestShotSelectRender = {
    .begin = M_BeginDraw,
    .description = M_DescriptionDraw,
    .background = M_BackgroundDraw,
    .picture = M_PictureDraw,
    .darken = M_DarkenDraw,
};

void TestShotSelect_Begin(
    TH02_SESSION *const session, TEST_SHOT_TRACE *const trace)
{
    static const SHOT_SELECT_ASSETS assets;
    m_Trace = trace;
    ShotSelect_Init(&g_TestShotSelectRender);
    p_Select->session = session;
    p_Select->assets = trace != nullptr ? &assets : nullptr;
    ShotSelect_BeginInput();
}
