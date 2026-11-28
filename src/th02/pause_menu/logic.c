#include "th02/pause_menu/private.h"

#include <string.h>

static PAUSE m_Pause;
PAUSE *const p_Pause = &m_Pause;
static const PAUSE_MENU_RENDER *m_Render;

static void M_Draw(
    const PAUSE_MENU_DRAW_KIND kind, const uint16_t x, const uint16_t y,
    const uint16_t attribute)
{
    if (m_Pause.assets != nullptr) {
        m_Render->draw(m_Pause.assets, kind, x, y, attribute);
    }
}

static void M_Tone(const int16_t tone)
{
    if (m_Pause.assets != nullptr) {
        m_Render->tone(tone);
    }
}

static void M_Flip(void)
{
    m_Pause.selected = 1 - m_Pause.selected;
    m_Pause.first_color =
        m_Pause.selected == 0 ? (TRAM_WHITE | TRAM_UNDERLINE) : TRAM_MAGENTA;
    m_Pause.second_color =
        m_Pause.selected == 0 ? TRAM_MAGENTA : (TRAM_WHITE | TRAM_UNDERLINE);
}

static void M_Choices(void)
{
    M_Draw(PAUSE_MENU_RESUME_TEXT, 23, 15, TRAM_WHITE | TRAM_UNDERLINE);
    M_Draw(PAUSE_MENU_QUIT_TEXT, 23, 16, TRAM_MAGENTA);
}

static void M_ClearPrompt(void)
{
    for (uint16_t y = 14; y <= 16; y++) {
        M_Draw(PAUSE_MENU_BLANK, 17, y, TRAM_WHITE);
    }
}

static PAUSE_MENU_RESULT M_Complete(const PAUSE_MENU_RESULT result)
{
    m_Pause.phase = PAUSE_MENU_DONE;
    m_Pause.result = result;
    return result;
}

static PAUSE_MENU_RESULT M_Update(const uint16_t keys)
{
    if (m_Pause.phase == PAUSE_MENU_DONE) {
        return m_Pause.result;
    }
    if (m_Pause.phase == PAUSE_MENU_SHOW) {
        M_Draw(
            (m_Pause.frame & 63u) < 50 ? PAUSE_MENU_HEADING : PAUSE_MENU_BLANK,
            18, 12, 0xe1);
        m_Pause.phase = PAUSE_MENU_READ;
        return PAUSE_MENU_INPUT;
    }
    m_Pause.keys = keys;
    m_Pause.frame++;
    if (m_Pause.state == 0 && keys == 0) {
        m_Pause.state = 1;
    }
    if (m_Pause.state == 1) {
        if ((keys & (GAME_KEY_UP | GAME_KEY_DOWN)) != 0) {
            M_Flip();
            M_Draw(PAUSE_MENU_RESUME_TEXT, 23, 15, m_Pause.first_color);
            M_Draw(PAUSE_MENU_QUIT_TEXT, 23, 16, m_Pause.second_color);
            m_Pause.state = 0;
        }
        if ((keys & (GAME_KEY_OK | GAME_KEY_SHOT)) != 0) {
            if (m_Pause.selected == 0) {
                m_Pause.state = 2;
            } else {
                M_Draw(PAUSE_MENU_PROMPT, 17, 14, TRAM_WHITE | TRAM_BLINK);
                M_Draw(PAUSE_MENU_NO_TEXT, 17, 15, TRAM_WHITE);
                M_Draw(PAUSE_MENU_YES_TEXT, 17, 16, TRAM_MAGENTA);
                m_Pause.state = 3;
                m_Pause.selected = 0;
            }
        }
        if ((keys & GAME_KEY_CANCEL) != 0) {
            m_Pause.state = 2;
        }
    } else if (m_Pause.state == 2) {
        if (keys == 0) {
            M_Tone(100);
            m_Pause.keys = 0;
            M_Draw(PAUSE_MENU_BLANK, 18, 12, TRAM_WHITE);
            M_ClearPrompt();
            return M_Complete(PAUSE_MENU_RESUME);
        }
    } else if (m_Pause.state == 3) {
        if (keys == 0) {
            m_Pause.state++;
        }
    } else if (m_Pause.state == 4) {
        if ((keys & (GAME_KEY_UP | GAME_KEY_DOWN)) != 0) {
            M_Flip();
            M_Draw(PAUSE_MENU_NO_TEXT, 17, 15, m_Pause.first_color);
            M_Draw(PAUSE_MENU_YES_TEXT, 17, 16, m_Pause.second_color);
            m_Pause.state = 3;
        }
        if ((keys & (GAME_KEY_OK | GAME_KEY_SHOT)) != 0) {
            if (m_Pause.selected == 0) {
                M_ClearPrompt();
                M_Choices();
                m_Pause.state = 0;
            } else {
                return M_Complete(PAUSE_MENU_QUIT);
            }
        }
        if ((keys & GAME_KEY_CANCEL) != 0) {
            m_Pause.state = 2;
        }
    }
    m_Pause.phase = PAUSE_MENU_SHOW;
    return PAUSE_MENU_FRAME;
}

void PauseMenu_Init(const PAUSE_MENU_RENDER *const render)
{
    memset(&m_Pause, 0, sizeof(m_Pause));
    m_Render = render;
}

void PauseMenu_Begin(const PAUSE_MENU_ASSETS *const assets, const uint16_t keys)
{
    m_Pause.assets = assets;
    m_Pause.keys = keys;
    m_Pause.frame = 0;
    m_Pause.state = 0;
    m_Pause.phase = PAUSE_MENU_SHOW;
    m_Pause.result = PAUSE_MENU_FRAME;
    M_Choices();
    M_Tone(70);
}

PAUSE_MENU_EVENT PauseMenu_Update(const uint16_t keys)
{
    const PAUSE_MENU_RESULT result = M_Update(keys);
    return (PAUSE_MENU_EVENT) { .result = result, .keys = m_Pause.keys };
}
