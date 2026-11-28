#include "th02/shot_select/logic.h"

#include "th02/shot_select/private.h"

#include "core/base/assert.h"

#include <string.h>

static const int16_t m_Left[3] = { 16, 224, 432 };
static const int16_t m_Top[3] = { 128, 224, 128 };

static SHOT_SELECT m_Select;
SHOT_SELECT *const p_Select = &m_Select;
static const SHOT_SELECT_RENDER *m_Render;

static void M_Description(const uint8_t color)
{
    if (m_Select.assets != nullptr) {
        ASSERT(!m_Select.fade.active);
        m_Render->description(m_Select.assets, m_Select.selection, color);
    }
}

static void M_Background(const bool highlight)
{
    if (m_Select.assets != nullptr) {
        ASSERT(!m_Select.fade.active);
        m_Render->background(m_Select.selection, highlight);
    }
}

static void M_Picture(const bool offset)
{
    if (m_Select.assets != nullptr) {
        ASSERT(!m_Select.fade.active);
        m_Render->picture(
            m_Select.assets, m_Select.selection,
            m_Left[m_Select.selection] + (offset ? 8 : 0),
            m_Top[m_Select.selection] + (offset ? 8 : 0));
    }
}

static void M_Darken(const bool offset)
{
    if (m_Select.assets != nullptr) {
        ASSERT(!m_Select.fade.active);
        m_Render->darken(
            m_Left[m_Select.selection] + (offset ? 8 : 0),
            m_Top[m_Select.selection] + (offset ? 8 : 0));
    }
}

static SHOT_SELECT_EVENTS M_Frame(const SHOT_SELECT_PHASE next)
{
    m_Select.phase = next;
    return (SHOT_SELECT_EVENTS) { SHOT_SELECT_FRAME, 0 };
}

static SHOT_SELECT_EVENTS M_Start(const bool right)
{
    m_Select.right_animation = right;
    if (right) {
        M_Background(false);
    } else {
        M_Description(7);
    }
    return M_Frame(SHOT_SELECT_OLD_FIRST);
}

static SHOT_SELECT_EVENTS M_Finish(void)
{
    if ((m_Select.keys & (GAME_KEY_SHOT | GAME_KEY_OK)) != 0) {
        m_Select.session->shot_type = (uint8_t)m_Select.selection;
        m_Select.phase = SHOT_SELECT_DONE;
        return (SHOT_SELECT_EVENTS) { SHOT_SELECT_FADE_OUT, 7 };
    }
    return M_Frame(SHOT_SELECT_TAIL);
}

void ShotSelect_Init(const SHOT_SELECT_RENDER *const render)
{
    memset(&m_Select, 0, sizeof(m_Select));
    m_Render = render;
}

void ShotSelect_Begin(
    TH02_SESSION *const session, const SHOT_SELECT_ASSETS *const assets,
    const bool cleared[3])
{
    ShotSelect_Init(m_Render);
    m_Select.session = session;
    m_Select.assets = assets;
    m_Render->begin(session, assets, cleared);
    Palette_BeginFade(&m_Select.fade, PALETTE_BLACK_IN, 2);
}

void ShotSelect_BeginInput(void)
{
    m_Select.keys = m_Select.input_locked = m_Select.input_delay = 0;
    m_Select.selection = 1;
    m_Select.phase = SHOT_SELECT_READ;
    m_Select.right_animation = false;
}

SHOT_SELECT_EVENTS ShotSelect_Update(const uint16_t keys)
{
    ASSERT(m_Select.selection >= 0 && m_Select.selection < 3);
    switch (m_Select.phase) {
    case SHOT_SELECT_READ:
        m_Select.keys = keys;
        if (m_Select.input_locked != 0) {
            return M_Frame(SHOT_SELECT_TAIL);
        }
        if ((keys & GAME_KEY_LEFT) != 0) {
            return M_Start(false);
        }
        if ((keys & GAME_KEY_RIGHT) != 0) {
            return M_Start(true);
        }
        return M_Finish();
    case SHOT_SELECT_OLD_FIRST:
        if (m_Select.right_animation) {
            M_Picture(true);
        } else {
            M_Background(false);
        }
        return M_Frame(SHOT_SELECT_OLD_SECOND);
    case SHOT_SELECT_OLD_SECOND:
        if (m_Select.right_animation) {
            M_Description(7);
        } else {
            M_Picture(true);
        }
        return M_Frame(SHOT_SELECT_OLD_THIRD);
    case SHOT_SELECT_OLD_THIRD:
        M_Darken(true);
        m_Select.selection = m_Select.right_animation
            ? (int8_t)((m_Select.selection + 1) % 3)
            : m_Select.selection == 0 ? 2
                                      : m_Select.selection - 1;
        return M_Frame(SHOT_SELECT_NEW_COPY);
    case SHOT_SELECT_NEW_COPY:
        M_Background(true);
        return M_Frame(SHOT_SELECT_NEW_DESCRIPTION);
    case SHOT_SELECT_NEW_DESCRIPTION:
        M_Description(12);
        return M_Frame(SHOT_SELECT_NEW_PICTURE);
    case SHOT_SELECT_NEW_PICTURE:
        M_Picture(false);
        if (!m_Select.right_animation
            && (m_Select.keys & GAME_KEY_RIGHT) != 0) {
            return M_Start(true);
        }
        return M_Finish();
    case SHOT_SELECT_TAIL:
        m_Select.input_locked = m_Select.keys;
        if (m_Select.input_locked != 0) {
            m_Select.input_delay = (uint16_t)(m_Select.input_delay + 1u);
            if (m_Select.input_delay > 30) {
                m_Select.input_locked = 0;
            }
        } else {
            m_Select.input_delay = 0;
        }
        m_Select.phase = SHOT_SELECT_READ;
        return (SHOT_SELECT_EVENTS) { SHOT_SELECT_INPUT, 0 };
    case SHOT_SELECT_DONE:
        return (SHOT_SELECT_EVENTS) { SHOT_SELECT_COMPLETE, 0 };
    }
    ASSERT(false);
    return (SHOT_SELECT_EVENTS) { SHOT_SELECT_COMPLETE, 0 };
}

void ShotSelect_BeginFadeOut(void)
{
    Palette_BeginFade(&m_Select.fade, PALETTE_BLACK_OUT, 1);
}

bool ShotSelect_VSync(void)
{
    return Palette_FadeVSync(&m_Select.fade);
}
