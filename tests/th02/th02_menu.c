#include "tests/th02/th02_menu.h"

static TEST_MENU_TRACE *m_Trace;

static void M_InitDraw(
    const TITLE_ASSETS *assets, const TH02_SETTINGS *settings,
    TRAM_CONSOLE *console, uint8_t attribute);
static void M_PutDraw(
    const TH02_SETTINGS *settings, bool unlocked, int8_t selection,
    uint16_t attribute);
static void M_ClearDraw(TRAM_CONSOLE *console, uint8_t attribute);
static void M_RestoreDraw(const TITLE_ASSETS *assets);

#define MainMenuRender_Init M_InitDraw
#define MainMenuRender_Put M_PutDraw
#define Menu_Clear M_ClearDraw
#define MainMenuRender_Restore M_RestoreDraw
#include "th02/main_menu/logic.c"
#undef MainMenuRender_Init
#undef MainMenuRender_Put
#undef Menu_Clear
#undef MainMenuRender_Restore

static void M_InitDraw(
    const TITLE_ASSETS *const assets, const TH02_SETTINGS *const settings,
    TRAM_CONSOLE *const console, const uint8_t attribute)
{
    if (m_Trace != nullptr) {
        m_Trace->init++;
        return;
    }
    MainMenuRender_Init(assets, settings, console, attribute);
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
    MainMenuRender_Put(settings, unlocked, selection, attribute);
}

static void M_ClearDraw(TRAM_CONSOLE *const console, const uint8_t attribute)
{
    if (m_Trace != nullptr) {
        m_Trace->clear++;
        return;
    }
    Menu_Clear(console, attribute);
}

static void M_RestoreDraw(const TITLE_ASSETS *const assets)
{
    if (m_Trace != nullptr) {
        m_Trace->restore++;
        return;
    }
    MainMenuRender_Restore(assets);
}

void TestMenu_Begin(
    TH02_SESSION *const session, TEST_MENU_TRACE *const trace,
    const bool unlocked, const uint8_t demo)
{
    static const TITLE_ASSETS assets;
    static TH02_SETTINGS settings;
    m_Trace = trace;
    MainMenu_Begin(
        session, &settings, trace != nullptr ? &assets : nullptr, nullptr,
        nullptr, nullptr, unlocked, demo);
}
