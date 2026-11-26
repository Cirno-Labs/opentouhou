#include "th02/options_menu/logic.h"

#include "th02/input.h"

#include <string.h>

typedef struct {
    const OPTIONS_MENU_CONTEXT *context;
    bool initialized;
    bool input_allowed;
} OPTIONS_MENU;

static OPTIONS_MENU m_Options;

static void M_Put(const int8_t selection, const uint16_t attribute)
{
    if (m_Options.context->assets != nullptr
        && m_Options.context->text != nullptr) {
        OptionsMenuRender_Put(
            m_Options.context->settings, m_Options.context->assets->glyphs,
            m_Options.context->text, selection, attribute);
    }
}

static void M_Emit(
    OPTIONS_MENU_SOUND *const sound, const OPTIONS_MENU_SOUND_ACTION action)
{
    if (sound->emit != nullptr) {
        const OPTIONS_MENU_SOUND_EVENT event = {
            action,
            sound->midi_active,
            sound->active,
            sound->fm_possible,
        };
        sound->emit(sound->context, &event);
    }
}

static void M_Determine(OPTIONS_MENU_SOUND *const sound)
{
    M_Emit(sound, OPTIONS_MENU_SOUND_DETERMINE);
    if (sound->fm_available) {
        sound->fm_possible = true;
    }
    sound->active = sound->fm_available || sound->midi_active;
}

static void M_Restart(void)
{
    OPTIONS_MENU_SOUND *const sound = m_Options.context->sound;
    if (m_Options.context->settings->music == TH02_MUSIC_OFF) {
        sound->fm_possible = false;
        M_Emit(sound, OPTIONS_MENU_SOUND_STOP);
        sound->active = false;
    } else {
        M_Emit(sound, OPTIONS_MENU_SOUND_STOP);
        sound->midi_active =
            m_Options.context->settings->music == TH02_MUSIC_MIDI
            && sound->midi_possible;
        M_Determine(sound);
        M_Emit(sound, OPTIONS_MENU_SOUND_PLAY);
    }
}

static int32_t M_Ring(
    const int32_t value, const int32_t maximum, const int32_t direction)
{
    const int32_t next = value + direction;
    return next < 0 ? maximum : next > maximum ? 0 : next;
}

static void M_Change(const int32_t direction)
{
    const int8_t selection = m_Options.context->nav->selection;
    TH02_SETTINGS *const settings = m_Options.context->settings;
    M_Put(selection, TRAM_YELLOW);
    switch (selection) {
    case 0:
        settings->difficulty = M_Ring(settings->difficulty, 3, direction);
        break;
    case 1:
        settings->music = M_Ring(settings->music, 2, direction);
        M_Restart();
        break;
    case 2:
        settings->lives = M_Ring(settings->lives, 4, direction);
        break;
    case 3:
        settings->bombs = M_Ring(settings->bombs, 3, direction);
        break;
    case 4:
        m_Options.context->session->reduce_effects =
            !m_Options.context->session->reduce_effects;
        settings->reduce_effects = m_Options.context->session->reduce_effects;
        break;
    }
    M_Put(selection, TRAM_WHITE);
}

static void M_Reset(void)
{
    TH02_SETTINGS *const settings = m_Options.context->settings;
    TH02_SESSION *const session = m_Options.context->session;
    settings->difficulty = TH02_DIFFICULTY_NORMAL;
    settings->music = TH02_MUSIC_FM;
    M_Restart();
    settings->lives = 2;
    settings->bombs = 3;
    session->unused_2 = 1;
    session->reduce_effects = false;
    settings->reduce_effects = false;
    for (int8_t i = 0; i < 5; i++) {
        M_Put(i, TRAM_YELLOW);
    }
}

static void M_Quit(void)
{
    m_Options.context->nav->selection = 3;
    m_Options.context->nav->in_option = false;
    m_Options.initialized = false;
}

void OptionsMenu_Init(void)
{
    memset(&m_Options, 0, sizeof(m_Options));
}

void OptionsMenu_Begin(const OPTIONS_MENU_CONTEXT *const context)
{
    memset(&m_Options, 0, sizeof(m_Options));
    m_Options.context = context;
}

OPTIONS_MENU_SAMPLE OptionsMenu_Sample(void)
{
    return (OPTIONS_MENU_SAMPLE) {
        .ready = m_Options.initialized,
        .input = m_Options.input_allowed,
    };
}

OPTIONS_MENU_EVENTS OptionsMenu_Update(const uint16_t keys)
{
    OPTIONS_MENU_EVENTS events = {};
    MENU_NAV *const nav = m_Options.context->nav;
    if (!nav->in_option) {
        return events;
    }
    if (!m_Options.initialized) {
        m_Options.initialized = true;
        m_Options.input_allowed = false;
        if (m_Options.context->assets != nullptr
            && m_Options.context->text != nullptr) {
            OptionsMenuRender_Init(
                m_Options.context->assets->glyphs, m_Options.context->text,
                m_Options.context->console, m_Options.context->clear_attribute);
        }
        for (int8_t i = 0; i < 7; i++) {
            M_Put(i, nav->selection == i ? TRAM_WHITE : TRAM_YELLOW);
        }
    }
    if (keys == 0) {
        m_Options.input_allowed = true;
    }
    if (m_Options.input_allowed) {
        if ((keys & GAME_KEY_UP) != 0) {
            M_Put(nav->selection, TRAM_YELLOW);
            Menu_Move(nav, 6, -1);
            M_Put(nav->selection, TRAM_WHITE);
        }
        if ((keys & GAME_KEY_DOWN) != 0) {
            M_Put(nav->selection, TRAM_YELLOW);
            Menu_Move(nav, 6, 1);
            M_Put(nav->selection, TRAM_WHITE);
        }
        if ((keys & GAME_KEY_RIGHT) != 0) {
            M_Change(1);
        }
        if ((keys & GAME_KEY_LEFT) != 0) {
            M_Change(-1);
        }
        if ((keys & (GAME_KEY_SHOT | GAME_KEY_OK)) != 0) {
            if (nav->selection == 5) {
                M_Reset();
            } else if (nav->selection == 6) {
                M_Quit();
            }
        }
        if ((keys & GAME_KEY_CANCEL) != 0) {
            M_Quit();
        }
        if (keys != 0) {
            m_Options.input_allowed = false;
        }
    }
    m_Options.context->session->frame =
        (int32_t)((uint32_t)m_Options.context->session->frame + 1u);
    *m_Options.context->idle_frame =
        (uint16_t)(*m_Options.context->idle_frame + 1u);
    events.wait_frame = true;
    events.return_main = !nav->in_option;
    return events;
}
