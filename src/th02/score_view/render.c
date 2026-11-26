#include "th02/score_view/render.h"

#include "th02/gaiji.h"
#include "th02/hud.h"

#include "core/base/assert.h"

#include <string.h>

static const uint8_t m_Title[] = { 0xb1, 0xb2, 0xcf, 0xbc, 0xac,
                                   0xb8, 0xbb, 0xae, 0 };
static const uint8_t m_Ranks[4][8] = {
    { 0xcf, 0xae, 0xaa, 0xbc, 0xc2, 0xcf, 0xcf, 0 },
    { 0xb6, 0xb8, 0xbb, 0xb7, 0xaa, 0xb5, 0xcf, 0 },
    { 0xcf, 0xb1, 0xaa, 0xbb, 0xad, 0xcf, 0xcf, 0 },
    { 0xb5, 0xbe, 0xb6, 0xaa, 0xbd, 0xb2, 0xac, 0 },
};

static void M_Character(const uint16_t x, const uint16_t y, const uint8_t code)
{
    const TRAM_CHARACTER character = { .code = code };
    Tram_PutCharacter(x, y, &character, TRAM_WHITE);
}

static void M_Number(
    const uint16_t x, const uint16_t y, const int16_t value,
    const uint16_t digits)
{
    int16_t divisor = digits == 4 ? 1000 : 10;
    for (uint16_t i = 0; i < digits; i++) {
        M_Character(x + i, y, (uint8_t)('0' + value / divisor % 10));
        divisor /= 10;
    }
}

static void M_Table(
    const SCORES *const scores, const TH02_SETTINGS *const settings,
    const SCORE_VIEW_ASSETS *const assets)
{
    const int32_t rank = settings->difficulty;
    ASSERT(rank >= 0 && rank < 4);
    const SCORE_SECTION *const section = &scores->sections[rank];
    Tram_PutGaijiString(22, 2, m_Title, TRAM_GREEN);
    Tram_PutGaijiString(40, 2, m_Ranks[rank], TRAM_GREEN);
    Tram_PutText(8, 4, &assets->header, UINT16_MAX, TRAM_GREEN);
    for (int16_t i = 0; i < SCORES_PLACES; i++) {
        const SCORE_RECORD *const record = &section->records[i];
        ASSERT(memchr(record->name_glyphs, 0, SCORES_NAME_GLYPHS) != nullptr);
        ASSERT(record->shot_type < 3);
        const uint16_t y = 7 + i;
        Tram_PutGaijiString(12, y, record->name_glyphs, TRAM_WHITE);
        Hud_Number(26, y, 8, record->score, TRAM_WHITE, false);
        Tram_PutGaiji(
            44, y,
            record->stage == 127 ? GAIJI_ALL : GAIJI_DIGIT_0 + record->stage,
            TRAM_WHITE);
        Tram_PutText(
            48, y, &assets->shot_types[record->shot_type], UINT16_MAX,
            TRAM_WHITE);
        M_Number(56, y, record->year, 4);
        M_Character(60, y, '/');
        M_Number(61, y, (int8_t)record->month, 2);
        M_Character(63, y, '/');
        M_Number(64, y, (int8_t)record->day, 2);
    }
    for (int16_t i = 0; i < SCORES_PLACES - 1; i++) {
        Tram_PutGaiji(9, 7 + i, GAIJI_DIGIT_0 + 1 + i, TRAM_WHITE);
    }
    Tram_PutGaiji(8, 16, GAIJI_DIGIT_0 + 1, TRAM_WHITE);
    Tram_PutGaiji(10, 16, GAIJI_DIGIT_0, TRAM_WHITE);
}

static void M_Fill(void)
{
    Grcg_SetColor(10);
    Draw_BoxFill(
        g_Clip, g_Clip->left, g_Clip->top, g_Clip->right, g_Clip->bottom);
    Grcg_Off();
}

void ScoreViewRender_Begin(
    const SCORES *const scores, const TH02_SETTINGS *const settings,
    const SCORE_VIEW_ASSETS *const assets)
{
    Video_AccessPage(0);
    Video_Clear();
    Video_AccessPage(1);
    Video_Clear();
    Palette_Import(assets->palette);
    Palette_Show();
    Clip_SetScoreTable();
    M_Fill();
    M_Table(scores, settings, assets);
    Video_AccessPage(0);
    Video_ShowPage(1);
}

void ScoreViewRender_Frame(
    const SCORE_VIEW_ASSETS *const assets, const int16_t frame)
{
    M_Fill();
    for (int16_t offset = 0; offset <= 320; offset += 320) {
        for (int16_t i = 0; i < 4; i++) {
            int16_t x = (int16_t)(frame + 160 * i + offset);
            x %= 640;
            int16_t y = (int16_t)(100 * i - frame);
            while (y < 0) {
                y += 400;
            }
            Draw_SpriteRect(&assets->logo[0], x, y);
            Draw_SpriteRect(&assets->logo[1], x + 64, y);
        }
    }
}

void ScoreViewRender_Flip(
    const uint8_t drawing_page, const uint8_t visible_page)
{
    Video_AccessPage(drawing_page);
    Video_ShowPage(visible_page);
}

void ScoreViewRender_End(void)
{
    Clip_Reset();
}
