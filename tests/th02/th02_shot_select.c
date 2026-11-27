#include "th02_shot_select.h"

static TEST_SHOT_TRACE *m_Trace;

static void M_DescriptionDraw(
    const SHOT_SELECT_ASSETS *assets, uint8_t selection, uint8_t color);
static void M_BackgroundDraw(uint8_t selection, bool highlight);
static void M_PictureDraw(
    const SHOT_SELECT_ASSETS *assets, uint8_t selection, int16_t left,
    int16_t top);
static void M_DarkenDraw(int16_t left, int16_t top);

#define ShotSelectRender_Description M_DescriptionDraw
#define ShotSelectRender_Background M_BackgroundDraw
#define ShotSelectRender_Picture M_PictureDraw
#define ShotSelectRender_Darken M_DarkenDraw
#include "../../src/th02/shot_select/logic.c"
#undef ShotSelectRender_Description
#undef ShotSelectRender_Background
#undef ShotSelectRender_Picture
#undef ShotSelectRender_Darken

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

static void M_DescriptionDraw(
    const SHOT_SELECT_ASSETS *const assets, const uint8_t selection,
    const uint8_t color)
{
    if (m_Trace != nullptr) {
        M_Record(SHOT_SELECT_DESCRIPTION, selection, 0, 0, color);
        return;
    }
    ShotSelectRender_Description(assets, selection, color);
}

static void M_BackgroundDraw(const uint8_t selection, const bool highlight)
{
    if (m_Trace != nullptr) {
        M_Record(SHOT_SELECT_BACKGROUND, selection, 0, 0, highlight);
        return;
    }
    ShotSelectRender_Background(selection, highlight);
}

static void M_PictureDraw(
    const SHOT_SELECT_ASSETS *const assets, const uint8_t selection,
    const int16_t left, const int16_t top)
{
    if (m_Trace != nullptr) {
        M_Record(SHOT_SELECT_PICTURE, selection, left, top, 0);
        return;
    }
    ShotSelectRender_Picture(assets, selection, left, top);
}

static void M_DarkenDraw(const int16_t left, const int16_t top)
{
    if (m_Trace != nullptr) {
        M_Record(SHOT_SELECT_DARKEN, m_Select.selection, left, top, 0);
        return;
    }
    ShotSelectRender_Darken(left, top);
}

void TestShotSelect_Begin(
    TH02_SESSION *const session, TEST_SHOT_TRACE *const trace)
{
    static const SHOT_SELECT_ASSETS assets;
    m_Trace = trace;
    ShotSelect_Init();
    m_Select.session = session;
    m_Select.assets = trace != nullptr ? &assets : nullptr;
    ShotSelect_BeginInput();
}
