#include "th02/game_over.h"

#include "th02/game_over_private.h"
#include "th02/hud.h"
#include "th02/input.h"
#include "th02/palette.h"
#include "th02/tram.h"
#include "th02/wait.h"

#include "core/base/assert.h"

#include <string.h>

static const uint8_t m_GameOverText[] = { 0xb0, 0xaa, 0xb7, 0xae, 0xb8,
                                          0xbf, 0xae, 0xbb, 0 };
static const uint8_t m_Continue[] = { 0xac, 0xb8, 0xb6, 0xbd, 0xb2,
                                      0xb6, 0xbe, 0xae, 0xdd, 0 };
static const uint8_t m_Yes[] = { 0xc2, 0xae, 0xbc, 0 };
static const uint8_t m_No[] = { 0xb6, 0xb8, 0 };
static const uint8_t m_Credit[] = { 0xac, 0xbb, 0xae, 0xad, 0xb2, 0xbd, 0 };

static GAME_OVER m_GameOver;
GAME_OVER *const p_GameOver = &m_GameOver;

static void M_Blank(const uint16_t x, const uint16_t y)
{
    const TRAM_CHARACTER space = { .code = ' ' };
    for (uint16_t i = 0; i < 16; i++) {
        Tram_PutCharacter(x + i, y, &space, TRAM_WHITE);
    }
}

static void M_Animate(void)
{
    const uint16_t i = m_GameOver.row;
    M_Blank(18, i + 1);
    Tram_PutGaijiString(18, i, m_GameOverText, TRAM_WHITE);
    M_Blank(18, 23 - i);
    Tram_PutGaijiString(18, 24 - i, m_GameOverText, TRAM_WHITE);
    M_Blank(29 - i, 12);
    Tram_PutGaijiString(30 - i, 12, m_GameOverText, TRAM_WHITE);
    M_Blank(i + 7, 12);
    Tram_PutGaijiString(i + 6, 12, m_GameOverText, TRAM_WHITE);
    Wait_Delay(&m_GameOver.wait, 1);
}

static void M_Choices(void)
{
    Tram_PutGaijiString(
        24, 14, m_Yes,
        m_GameOver.yes ? (TRAM_WHITE | TRAM_REVERSE) : TRAM_WHITE);
    Tram_PutGaijiString(
        25, 15, m_No,
        m_GameOver.yes ? TRAM_WHITE : (TRAM_WHITE | TRAM_REVERSE));
}

void GameOver_Init(void)
{
    memset(&m_GameOver, 0, sizeof(m_GameOver));
}

void GameOver_Begin(const TH02_SESSION *const session)
{
    GameOver_Init();
    m_GameOver.session = session;
    m_GameOver.row = 21;
    m_GameOver.phase = GAME_OVER_COVER;
    Hud_BeginWipe(&m_GameOver.wipe, true);
}

void GameOver_Resume(void)
{
    ASSERT(m_GameOver.phase == GAME_OVER_REGISTER);
    Hud_WipePlayfield();
    if (m_GameOver.session->continues_used >= 3
        || m_GameOver.session->rank >= SESSION_EXTRA_RANK) {
        m_GameOver.phase = GAME_OVER_DONE;
        m_GameOver.event = GAME_OVER_EXIT;
        return;
    }
    Tram_PutGaijiString(18, 12, m_Continue, TRAM_WHITE);
    m_GameOver.yes = true;
    M_Choices();
    Tram_PutGaijiString(18, 20, m_Credit, TRAM_GREEN);
    Tram_PutGaiji(
        32, 20, GAIJI_DIGIT_0 + 3 - m_GameOver.session->continues_used,
        TRAM_GREEN);
    m_GameOver.phase = GAME_OVER_CHOICE;
    m_GameOver.event = GAME_OVER_WAIT;
    Wait_Cancel(&m_GameOver.wait);
}

GAME_OVER_EVENT GameOver_Update(const uint16_t keys)
{
    for (;;) {
        switch (m_GameOver.phase) {
        case GAME_OVER_COVER:
            if (!Hud_UpdateWipe(&m_GameOver.wipe)) {
                return GAME_OVER_WAIT;
            }
            Palette_SetTone(50);
            Hud_BeginWipe(&m_GameOver.wipe, false);
            m_GameOver.phase = GAME_OVER_REVEAL;
            return GAME_OVER_WAIT;
        case GAME_OVER_REVEAL:
            if (!Hud_UpdateWipe(&m_GameOver.wipe)) {
                return GAME_OVER_WAIT;
            }
            M_Animate();
            m_GameOver.phase = GAME_OVER_ANIMATE;
            return GAME_OVER_WAIT;
        case GAME_OVER_ANIMATE:
            if (!Wait_Ready(&m_GameOver.wait, nullptr, 0)) {
                return GAME_OVER_WAIT;
            }
            if (m_GameOver.row > 12) {
                m_GameOver.row--;
                M_Animate();
                return GAME_OVER_WAIT;
            }
            Wait_Delay(&m_GameOver.wait, 30);
            m_GameOver.phase = GAME_OVER_DELAY;
            return GAME_OVER_WAIT;
        case GAME_OVER_DELAY:
            if (!Wait_Ready(&m_GameOver.wait, nullptr, 0)) {
                return GAME_OVER_WAIT;
            }
            m_GameOver.phase = GAME_OVER_KEY;
            break;
        case GAME_OVER_KEY:
            if (keys == 0) {
                return GAME_OVER_WAIT;
            }
            Hud_WipePlayfield();
            m_GameOver.phase = GAME_OVER_REGISTER;
            return GAME_OVER_NAME;
        case GAME_OVER_REGISTER:
            return GAME_OVER_NAME;
        case GAME_OVER_CHOICE:
            if (!Wait_Ready(&m_GameOver.wait, nullptr, 0)) {
                return GAME_OVER_WAIT;
            }
            if (m_GameOver.input_state == 0 && keys == 0) {
                m_GameOver.input_state = 1;
            } else if (m_GameOver.input_state == 1 && keys != 0) {
                m_GameOver.input_state = 2;
            }
            if ((keys & GAME_KEY_UP) != 0) {
                m_GameOver.yes = true;
                M_Choices();
            }
            if ((keys & GAME_KEY_DOWN) != 0) {
                m_GameOver.yes = false;
                M_Choices();
            }
            if (m_GameOver.input_state == 2
                && (keys & (GAME_KEY_SHOT | GAME_KEY_OK | GAME_KEY_CANCEL))
                    != 0) {
                m_GameOver.yes &= (keys & GAME_KEY_CANCEL) == 0;
                m_GameOver.event =
                    m_GameOver.yes ? GAME_OVER_RESUME : GAME_OVER_EXIT;
                m_GameOver.phase = GAME_OVER_DONE;
                return m_GameOver.event;
            }
            Wait_Delay(&m_GameOver.wait, 1);
            return GAME_OVER_WAIT;
        case GAME_OVER_DONE:
            return m_GameOver.event;
        }
    }
}
