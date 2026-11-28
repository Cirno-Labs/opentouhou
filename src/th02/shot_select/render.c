#include "th02/shot_select/render.h"

#include "core/base/assert.h"

static void M_Text(
    const DRAW_TEXT *const text, const int16_t left, const int16_t top,
    const uint8_t color)
{
    Draw_Text(text->glyphs, text->count, left, top, color | 0x20);
}

static void M_Panel(
    const int16_t left, const int16_t top, const int16_t right,
    const int16_t bottom, const uint8_t color)
{
    Grcg_SetColor(color);
    Draw_RoundBoxFill(g_Clip, left + 8, top + 8, right + 8, bottom + 8, 8);
    Grcg_SetColor(0);
    Draw_RoundBoxFill(g_Clip, left, top, right, bottom, 8);
    Grcg_Off();
}

static void M_Header(
    const TH02_SESSION *const session, const SHOT_SELECT_ASSETS *const assets)
{
    M_Panel(128, 16, 512, 48, 12);
    M_Text(&assets->choose, 144, 24, 12);
    if (session->stage == 5) {
        M_Panel(16, 48, 624, 96, 12);
        M_Text(&assets->extra_note[0], 32, 56, 15);
        M_Text(&assets->extra_note[1], 32, 72, 15);
    }
}

void ShotSelectRender_Picture(
    const SHOT_SELECT_ASSETS *const assets, const uint8_t selection,
    const int16_t left, const int16_t top)
{
    ASSERT(selection < 3);
    ASSERT(g_Grcg->mode == 0);
    const TITLE_FLASH_IMAGE *const image = &assets->portraits[selection];
    Video_PutImage8(
        g_Clip, image->pixels, image->width, image->height, left, top);
}

void ShotSelectRender_Darken(const int16_t left, const int16_t top)
{
    ASSERT(left >= 0 && (left & 7) == 0 && left + 192 <= VIDEO_WIDTH);
    ASSERT(top >= 0 && top + 144 <= VIDEO_HEIGHT);
    Grcg_SetColor(0);
    uint8_t *const page = g_Video.pages[g_Video.drawing_page];
    for (size_t y = 0; y < 144; y++) {
        for (size_t x = 0; x < 192; x++) {
            if (((x + y) & 1) != 0) {
                page[(top + y) * VIDEO_WIDTH + left + x] = 0;
            }
        }
    }
    Grcg_Off();
}

void ShotSelectRender_Background(const uint8_t selection, const bool highlight)
{
    ASSERT(selection < 3);
    static const int16_t left[3] = { 16, 224, 432 };
    static const int16_t top[3] = { 128, 224, 128 };
    ASSERT(g_Grcg->mode == 0);
    Video_RestoreRect16(
        left[selection] + (highlight ? 192 : 0),
        top[selection] + (highlight ? 8 : 0), 16, 144);
    Video_RestoreRect16(
        left[selection] + (highlight ? 8 : 0),
        top[selection] + (highlight ? 144 : 0), 192, highlight ? 8 : 10);
}

void ShotSelectRender_Description(
    const SHOT_SELECT_ASSETS *const assets, const uint8_t selection,
    const uint8_t color)
{
    ASSERT(color < 16);
    ASSERT(selection < 3);
    static const int16_t left[3] = { 16, 224, 432 };
    static const int16_t top[3] = { 296, 136, 296 };
    M_Panel(
        left[selection], top[selection], left[selection] + 192,
        top[selection] + 64, color);
    for (size_t i = 0; i < 3; i++) {
        M_Text(
            &assets->descriptions[selection][i], left[selection] + 8,
            top[selection] + 8 + i * 16, color);
    }
}

void ShotSelectRender_Begin(
    TH02_SESSION *const session, const SHOT_SELECT_ASSETS *const assets,
    const bool cleared[3])
{
    ASSERT(g_Grcg->mode == 0);
    Palette_SetTone(0);
    Video_AccessPage(0);
    Palette_Import(assets->background.palette);
    Palette_Show();
    Video_PutImage8(
        g_Clip, assets->background.view.pixels, assets->background.view.width,
        assets->background.view.height, 0, 0);
    Grcg_Off();
    Video_CopyPage(1);
    Video_AccessPage(0);
    for (size_t i = 0; i < 3; i++) {
        if (cleared[i]) {
            M_Text(&assets->cleared, 16 + i * 208, 112, 15);
        }
    }
    ShotSelectRender_Picture(assets, 0, 24, 136);
    ShotSelectRender_Picture(assets, 1, 224, 224);
    ShotSelectRender_Picture(assets, 2, 440, 136);
    session->shot_type = 1;
    ShotSelectRender_Darken(24, 136);
    ShotSelectRender_Darken(440, 136);
    ShotSelectRender_Description(assets, 0, 7);
    ShotSelectRender_Description(assets, 1, 12);
    ShotSelectRender_Description(assets, 2, 7);
    M_Header(session, assets);
}

const SHOT_SELECT_RENDER g_ShotSelectRender = {
    .begin = ShotSelectRender_Begin,
    .description = ShotSelectRender_Description,
    .background = ShotSelectRender_Background,
    .picture = ShotSelectRender_Picture,
    .darken = ShotSelectRender_Darken,
};
