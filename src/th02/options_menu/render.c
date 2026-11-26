#include "th02/options_menu/render.h"

#include "th02/menu.h"

#include "core/base/assert.h"

static const uint8_t m_Rank[] = { 0xbb, 0xaa, 0xb6, 0xb4, 0 };
static const uint8_t m_Music[] = { 0xb7, 0xbe, 0xbc, 0xb2, 0xac, 0 };
static const uint8_t m_Player[] = { 0xb9, 0xb5, 0xaa, 0xc2, 0xae, 0xbb, 0 };
static const uint8_t m_Bomb[] = { 0xab, 0xb8, 0xb7, 0xab, 0 };
static const uint8_t m_Reset[] = { 0xbb, 0xae, 0xbc, 0xae, 0xbd, 0 };
static const uint8_t m_Quit[] = { 0xba, 0xbe, 0xb2, 0xbd, 0 };
static const uint8_t *const m_Labels[] = { m_Rank, m_Music, m_Player, m_Bomb };
static const uint8_t m_Modes[3][5] = {
    { 0xb8, 0xaf, 0xaf, 0xcf, 0 },
    { 0xcf, 0xaf, 0xb7, 0xcf, 0 },
    { 0xb7, 0xb2, 0xad, 0xb2, 0 },
};

static void M_TextShadow(
    const OPTIONS_MENU_TEXT *const reduction, const size_t text,
    const int16_t x, const int16_t y)
{
    const OPTIONS_MENU_TEXT *const cached = &reduction[text];
    Draw_Text(cached->glyphs, cached->count, x, y, 0);
}

static void M_Value(
    const GAIJI *const glyphs, const int16_t selection,
    const uint8_t *const value, const int16_t length, const int16_t excess,
    const uint16_t attribute)
{
    const int16_t x = 400 - length * 8;
    const int16_t y = 256 + selection * 16;
    Tram_PutGaijiString(x / 8, y / 16, value, attribute);
    ASSERT(g_Grcg->mode == 0);
    Video_RestoreRect16(x, y + 4, length * 16 + excess, 16);
    Draw_GaijiString(glyphs, x + 4, y + 4, 16, value, 0);
}

void OptionsMenuRender_Init(
    const GAIJI *const glyphs, const OPTIONS_MENU_TEXT text[3],
    TRAM_CONSOLE *const console, const uint8_t clear_attribute)
{
    Menu_Clear(console, clear_attribute);
    Video_ShowPage(1);
    Grcg_Off();
    Video_CopyPage(0);
    for (size_t i = 0; i < 4; i++) {
        Draw_GaijiString(glyphs, 196, 260 + i * 16, 16, m_Labels[i], 0);
    }
    M_TextShadow(text, 0, 196, 324);
    Draw_GaijiString(glyphs, 284, 340, 16, m_Reset, 0);
    Draw_GaijiString(glyphs, 292, 372, 16, m_Quit, 0);
    Video_ShowPage(0);
}

void OptionsMenuRender_Put(
    const TH02_SETTINGS *const settings, const GAIJI *const glyphs,
    const OPTIONS_MENU_TEXT reduction[3], const int8_t selection,
    const uint16_t attribute)
{
    if (selection >= 0 && selection < 4) {
        Tram_PutGaijiString(24, 16 + selection, m_Labels[selection], attribute);
        if (selection == 0) {
            M_Value(
                glyphs, 0, Menu_DifficultyName(settings->difficulty), 8, 0,
                attribute);
        } else if (selection == 1) {
            ASSERT(settings->music >= 0 && settings->music < 3);
            M_Value(glyphs, 1, m_Modes[settings->music], 3, 16, attribute);
        } else {
            const int32_t digit =
                selection == 2 ? settings->lives + 1 : settings->bombs;
            ASSERT(digit >= 0 && digit <= 5);
            Tram_PutGaiji(49, 16 + selection, GAIJI_DIGIT_0 + digit, attribute);
            ASSERT(g_Grcg->mode == 0);
            Video_RestoreRect16(392, 260 + selection * 16, 32, 16);
            Draw_Gaiji(
                glyphs, 396, 260 + selection * 16, GAIJI_DIGIT_0 + digit, 0,
                false);
        }
    } else if (selection == 4) {
        const size_t value = settings->reduce_effects ? 2 : 1;
        Tram_PutText(24, 20, &reduction[0].cells, UINT16_MAX, attribute);
        Tram_PutText(45, 20, &reduction[value].cells, UINT16_MAX, attribute);
        ASSERT(g_Grcg->mode == 0);
        Video_RestoreRect16(360, 324, 128, 16);
        M_TextShadow(reduction, value, 364, 324);
    } else if (selection == 5) {
        Tram_PutGaijiString(35, 21, m_Reset, attribute);
    } else if (selection == 6) {
        Tram_PutGaijiString(36, 23, m_Quit, attribute);
    }
}
