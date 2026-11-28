#include "tests/th02/th02_pause_menu.h"

#include "th02/pause_menu/private.h"

static TEST_PAUSE_REQUESTS *m_Requests;

static void M_ObserveDraw(
    const PAUSE_MENU_ASSETS *const assets, const PAUSE_MENU_DRAW_KIND kind,
    const uint16_t x, const uint16_t y, const uint16_t attribute)
{
    if (m_Requests != nullptr) {
        m_Requests->draws[m_Requests->count++] =
            (TEST_PAUSE_DRAW) { kind, x, y, attribute };
    }
    PauseMenuRender_Draw(assets, kind, x, y, attribute);
}

static void M_ObserveTone(const int16_t tone)
{
    if (m_Requests != nullptr) {
        m_Requests->tone = tone;
    }
    Palette_SetTone(tone);
}

static const PAUSE_MENU_RENDER m_TestRender = {
    .draw = M_ObserveDraw,
    .tone = M_ObserveTone,
};

void TestPause_Observe(TEST_PAUSE_REQUESTS *const requests)
{
    m_Requests = requests;
}

void TestPause_Init(void)
{
    PauseMenu_Init(&m_TestRender);
}
