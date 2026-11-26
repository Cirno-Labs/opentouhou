#include "th02/main_menu/render.h"

#include "th02/gaiji.h"

#include "core/base/assert.h"

static const uint8_t m_Start[] = { 0xbc, 0xbd, 0xaa, 0xbb, 0xbd, 0 };
static const uint8_t m_Extra[] = { 0xae, 0xc1, 0xbd, 0xbb, 0xaa, 0xcf,
                                   0xbc, 0xbd, 0xaa, 0xbb, 0xbd, 0 };
static const uint8_t m_Scores[] = {
    0xb1, 0xb2, 0xbc, 0xac, 0xb8, 0xbb, 0xae, 0
};
static const uint8_t m_Options[] = { 0xb8, 0xb9, 0xbd, 0xb2, 0xb8, 0xb6, 0 };
static const uint8_t m_Music[] = { 0xb7, 0xbe, 0xbc, 0xb2, 0xac, 0xcf,
                                   0xb7, 0xb8, 0xad, 0xae, 0 };
static const uint8_t m_Quit[] = { 0xba, 0xbe, 0xb2, 0xbd, 0 };
static const uint8_t m_Rank[] = { 0xbb, 0xaa, 0xb6, 0xb4, 0 };
static const uint8_t *const m_Items[] = {
    m_Start, m_Extra, m_Scores, m_Options, m_Music, m_Quit,
};
static const uint8_t m_Lengths[] = { 5, 11, 7, 6, 10, 4 };

void MainMenuRender_Init(
    const TITLE_ASSETS *const assets, const TH02_SETTINGS *const settings,
    TRAM_CONSOLE *const console, const uint8_t clear_attribute)
{
    Menu_Clear(console, clear_attribute);
    Video_ShowPage(1);
    Grcg_Off();
    Video_CopyPage(0);
    for (size_t i = 0; i < 6; i++) {
        Draw_GaijiString(
            assets->glyphs, 324 - m_Lengths[i] * 8, 260 + i * 16, 16,
            m_Items[i], 0);
    }
    Draw_GaijiString(assets->glyphs, 212, 372, 16, m_Rank, 0);
    Draw_GaijiString(
        assets->glyphs, 308, 372, 16, Menu_DifficultyName(settings->difficulty),
        0);
    Video_ShowPage(0);
}

void MainMenuRender_Put(
    const TH02_SETTINGS *const settings, const bool extra_unlocked,
    const int8_t selection, const uint16_t attribute)
{
    if (selection >= 0 && selection < 6) {
        const uint16_t color =
            selection == 1 && !extra_unlocked ? TRAM_BLUE : attribute;
        Tram_PutGaijiString(
            40 - m_Lengths[selection], 16 + selection, m_Items[selection],
            color);
    }
    Tram_PutGaijiString(26, 23, m_Rank, TRAM_GREEN);
    Tram_PutGaijiString(
        38, 23, Menu_DifficultyName(settings->difficulty), TRAM_GREEN);
}

void MainMenuRender_Restore(const TITLE_ASSETS *const assets)
{
    const TITLE_IMAGE *const background = &assets->menu_background;
    ASSERT(g_Grcg->mode == 0);
    Video_AccessPage(1);
    Video_ShowPage(0);
    Palette_Import(background->palette);
    Palette_Show();
    Video_PutImage8(
        g_Clip, background->view.pixels, background->view.width,
        background->view.height, 0, 0);
    Palette_Import(assets->menu_palette);
    Palette_Show();
    Grcg_Off();
    Video_CopyPage(0);
    Video_AccessPage(0);
}
