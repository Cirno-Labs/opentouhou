#include "th02/main_menu/logic.h"

#include <string.h>

typedef struct {
    MENU_NAV nav;
    TH02_SESSION *session;
    TH02_SETTINGS *settings;
    const TITLE_ASSETS *assets;
    const OPTIONS_MENU_TEXT *option_text;
    OPTIONS_MENU_SOUND *sound;
    TRAM_CONSOLE *console;
    uint8_t clear_attribute;
    uint16_t idle_frame;
    uint8_t demo;
    bool initialized;
    bool input_allowed;
    bool quit;
    bool suspended;
    MAIN_MENU_ACTION pending;
} MAIN_MENU;

static MAIN_MENU m_Menu;
static OPTIONS_MENU_CONTEXT m_OptionsContext;

static void M_Put(const int8_t selection, const uint16_t attribute)
{
    if (m_Menu.assets != nullptr) {
        MainMenuRender_Put(
            m_Menu.settings, m_Menu.nav.extra_unlocked, selection, attribute);
    }
}

static MAIN_MENU_EVENTS M_Suspend(const MAIN_MENU_ACTION action)
{
    m_Menu.suspended = true;
    m_Menu.pending = action;
    return (MAIN_MENU_EVENTS) {
        .action = action,
        .score_frames = action == MAIN_MENU_SCORES ? 2000 : 0,
        .demo = action == MAIN_MENU_DEMO ? m_Menu.demo : 0,
        .quit = m_Menu.quit,
    };
}

static MAIN_MENU_EVENTS M_Finish(MAIN_MENU_EVENTS events)
{
    m_Menu.session->frame = (int32_t)((uint32_t)m_Menu.session->frame + 1u);
    m_Menu.idle_frame = (uint16_t)(m_Menu.idle_frame + 1u);
    events.wait_frame = true;
    events.quit = m_Menu.quit;
    return events;
}

static MAIN_MENU_EVENTS M_Tail(
    const uint16_t keys, const bool allowed, const MAIN_MENU_EVENTS events)
{
    if (allowed) {
        if ((keys & GAME_KEY_CANCEL) != 0) {
            m_Menu.quit = true;
        }
        if (keys != 0) {
            m_Menu.input_allowed = false;
            m_Menu.idle_frame = 0;
        }
    }
    if (m_Menu.idle_frame > 640) {
        return M_Suspend(MAIN_MENU_DEMO);
    }
    return M_Finish(events);
}

MAIN_MENU_SAMPLE MainMenu_Sample(void)
{
    const OPTIONS_MENU_SAMPLE options = OptionsMenu_Sample();
    return (MAIN_MENU_SAMPLE) {
        .selection = m_Menu.nav.selection,
        .idle_frame = m_Menu.idle_frame,
        .in_options = m_Menu.nav.in_option,
        .main_ready = m_Menu.initialized,
        .main_input = m_Menu.input_allowed,
        .options_ready = options.ready,
        .options_input = options.input,
    };
}

void MainMenu_Init(void)
{
    memset(&m_Menu, 0, sizeof(m_Menu));
    OptionsMenu_Init();
}

void MainMenu_Begin(
    TH02_SESSION *const session, TH02_SETTINGS *const settings,
    const TITLE_ASSETS *const assets,
    const OPTIONS_MENU_TEXT *const option_text, OPTIONS_MENU_SOUND *const sound,
    TRAM_CONSOLE *const console, const bool extra_unlocked, const uint8_t demo)
{
    memset(&m_Menu, 0, sizeof(m_Menu));
    m_Menu.session = session;
    m_Menu.settings = settings;
    m_Menu.assets = assets;
    m_Menu.option_text = option_text;
    m_Menu.sound = sound;
    m_Menu.console = console;
    m_Menu.clear_attribute =
        console != nullptr ? console->attribute : TRAM_WHITE;
    m_Menu.nav.extra_unlocked = extra_unlocked;
    m_Menu.demo = demo;
    memset(&m_OptionsContext, 0, sizeof(m_OptionsContext));
    m_OptionsContext.session = m_Menu.session;
    m_OptionsContext.settings = m_Menu.settings;
    m_OptionsContext.assets = m_Menu.assets;
    m_OptionsContext.text = m_Menu.option_text;
    m_OptionsContext.sound = m_Menu.sound;
    m_OptionsContext.console = m_Menu.console;
    m_OptionsContext.clear_attribute = m_Menu.clear_attribute;
    m_OptionsContext.nav = &m_Menu.nav;
    m_OptionsContext.idle_frame = &m_Menu.idle_frame;
    OptionsMenu_Begin(&m_OptionsContext);
}

MAIN_MENU_EVENTS MainMenu_Update(const uint16_t keys)
{
    MAIN_MENU_EVENTS events = { .quit = m_Menu.quit };
    if (m_Menu.nav.in_option) {
        const OPTIONS_MENU_EVENTS options = OptionsMenu_Update(keys);
        return (MAIN_MENU_EVENTS) { .wait_frame = options.wait_frame };
    }
    if (m_Menu.suspended) {
        return events;
    }
    if (!m_Menu.initialized) {
        m_Menu.input_allowed = false;
        m_Menu.initialized = true;
        if (m_Menu.assets != nullptr) {
            MainMenuRender_Init(
                m_Menu.assets, m_Menu.settings, m_Menu.console,
                m_Menu.clear_attribute);
        }
        m_Menu.idle_frame = 0;
        for (int8_t i = 0; i < 6; i++) {
            M_Put(i, m_Menu.nav.selection == i ? TRAM_WHITE : TRAM_YELLOW);
        }
    }
    if (keys == 0) {
        m_Menu.input_allowed = true;
    }
    if (m_Menu.input_allowed) {
        if ((keys & GAME_KEY_UP) != 0) {
            M_Put(m_Menu.nav.selection, TRAM_YELLOW);
            Menu_Move(&m_Menu.nav, 5, -1);
            M_Put(m_Menu.nav.selection, TRAM_WHITE);
        }
        if ((keys & GAME_KEY_DOWN) != 0) {
            M_Put(m_Menu.nav.selection, TRAM_YELLOW);
            Menu_Move(&m_Menu.nav, 5, 1);
            M_Put(m_Menu.nav.selection, TRAM_WHITE);
        }
        if ((keys & (GAME_KEY_SHOT | GAME_KEY_OK)) != 0) {
            switch (m_Menu.nav.selection) {
            case 0:
                return M_Suspend(MAIN_MENU_START);
            case 1:
                return M_Suspend(MAIN_MENU_EXTRA);
            case 2:
                if (m_Menu.assets != nullptr) {
                    Menu_Clear(m_Menu.console, m_Menu.clear_attribute);
                }
                return M_Suspend(MAIN_MENU_SCORES);
            case 3:
                m_Menu.nav.selection = 0;
                m_Menu.nav.in_option = true;
                m_Menu.initialized = false;
                events.action = MAIN_MENU_OPTIONS;
                break;
            case 4:
                if (m_Menu.assets != nullptr) {
                    Menu_Clear(m_Menu.console, m_Menu.clear_attribute);
                }
                return M_Suspend(MAIN_MENU_MUSIC);
            case 5:
                m_Menu.quit = true;
                break;
            }
        }
    }
    return M_Tail(keys, m_Menu.input_allowed, events);
}

MAIN_MENU_EVENTS MainMenu_Resume(const uint16_t keys)
{
    MAIN_MENU_EVENTS events = { .quit = m_Menu.quit };
    if (!m_Menu.suspended) {
        return events;
    }
    const MAIN_MENU_ACTION pending = m_Menu.pending;
    m_Menu.suspended = false;
    m_Menu.pending = MAIN_MENU_NONE;
    if (pending == MAIN_MENU_SCORES) {
        if (m_Menu.assets != nullptr) {
            MainMenuRender_Restore(m_Menu.assets);
        }
        m_Menu.initialized = false;
    } else if (pending == MAIN_MENU_MUSIC) {
        m_Menu.initialized = false;
    } else if (pending == MAIN_MENU_DEMO) {
        return M_Finish(events);
    }
    return M_Tail(keys, true, events);
}
