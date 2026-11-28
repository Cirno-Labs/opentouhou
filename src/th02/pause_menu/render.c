#include "th02/pause_menu/render.h"

#include "th02/palette.h"

#include "core/base/assert.h"

static const uint8_t m_Heading[] = {
    0xb9, 0xaa, 0xbe, 0xbc, 0xae, 0xcf, 0xb7, 0xae, 0xb6, 0xbe, 0,
};
static const uint8_t m_Blank[] = {
    0xcf, 0xcf, 0xcf, 0xcf, 0xcf, 0xcf, 0xcf, 0xcf, 0xcf, 0xcf, 0xcf, 0,
};

void PauseMenuRender_Draw(
    const PAUSE_MENU_ASSETS *const assets, const PAUSE_MENU_DRAW_KIND kind,
    const uint16_t x, const uint16_t y, const uint16_t attribute)
{
    ASSERT(kind >= PAUSE_MENU_RESUME_TEXT && kind <= PAUSE_MENU_BLANK);
    if (kind < PAUSE_MENU_HEADING) {
        Tram_PutText(x, y, &assets->labels[kind], UINT16_MAX, attribute);
    } else {
        Tram_PutGaijiString(
            x, y, kind == PAUSE_MENU_HEADING ? m_Heading : m_Blank, attribute);
    }
}

const PAUSE_MENU_RENDER g_PauseMenuRender = {
    .draw = PauseMenuRender_Draw,
    .tone = Palette_SetTone,
};
