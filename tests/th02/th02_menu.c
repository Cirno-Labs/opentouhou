#include "tests/th02/th02_menu.h"

#include "th02/main_menu/private.h"

static TEST_MENU_TRACE *m_Trace;

static void M_InitDraw(
    const TITLE_ASSETS *const assets, const TH02_SETTINGS *const settings,
    TRAM_CONSOLE *const console, const uint8_t attribute)
{
    if (m_Trace != nullptr) {
        m_Trace->init++;
        return;
    }
    g_MainMenuRender.init(assets, settings, console, attribute);
}

static void M_PutDraw(
    const TH02_SETTINGS *const settings, const bool unlocked,
    const int8_t selection, const uint16_t attribute)
{
    if (m_Trace != nullptr) {
        if (m_Trace->count < 32) {
            m_Trace->selections[m_Trace->count] = selection;
            m_Trace->attributes[m_Trace->count++] = attribute;
        }
        return;
    }
    g_MainMenuRender.put(settings, unlocked, selection, attribute);
}

static void M_ClearDraw(TRAM_CONSOLE *const console, const uint8_t attribute)
{
    if (m_Trace != nullptr) {
        m_Trace->clear++;
        return;
    }
    g_MainMenuRender.clear(console, attribute);
}

static void M_RestoreDraw(const TITLE_ASSETS *const assets)
{
    if (m_Trace != nullptr) {
        m_Trace->restore++;
        return;
    }
    g_MainMenuRender.restore(assets);
}

static const MAIN_MENU_RENDER m_TestRender = {
    .init = M_InitDraw,
    .put = M_PutDraw,
    .restore = M_RestoreDraw,
    .clear = M_ClearDraw,
};

void TestMenu_Begin(
    TH02_SESSION *const session, TEST_MENU_TRACE *const trace,
    const bool unlocked, const uint8_t demo)
{
    static const TITLE_ASSETS assets;
    static TH02_SETTINGS settings;
    m_Trace = trace;
    MainMenu_Init(&m_TestRender);
    MainMenu_Begin(
        session, &settings, trace != nullptr ? &assets : nullptr, nullptr,
        nullptr, nullptr, unlocked, demo);
}
