#include "th02_pause_menu.h"

static TEST_PAUSE_REQUESTS *m_Requests;

static void M_ObserveDraw(
    const PAUSE_MENU_ASSETS *assets, PAUSE_MENU_DRAW_KIND kind, uint16_t x,
    uint16_t y, uint16_t attribute);
static void M_ObserveTone(int16_t tone);

#define PauseMenuRender_Draw M_ObserveDraw
#define Palette_SetTone M_ObserveTone
#include "../src/th02/pause_menu/logic.c"
#undef PauseMenuRender_Draw
#undef Palette_SetTone

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

void TestPause_Observe(TEST_PAUSE_REQUESTS *const requests)
{
    m_Requests = requests;
}
