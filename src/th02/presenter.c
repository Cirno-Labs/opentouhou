#include "th02/presenter.h"

#include "core/gfx/draw.h"
#include "core/gfx/texture.h"

#include <string.h>

static GFX_TEXTURE *m_Layers[2];
static uint8_t *m_Pixels[2];

// clang-format off
static const GFX_COLOR m_TextPalette[9] = {
    { 0, 0, 0, 0 },         // Transparent
    { 0, 0, 0, 255 },       // Black
    { 0, 0, 255, 255 },     // Blue
    { 255, 0, 0, 255 },     // Red
    { 255, 0, 255, 255 },   // Magenta
    { 0, 255, 0, 255 },     // Green
    { 0, 255, 255, 255 },   // Cyan
    { 255, 255, 0, 255 },   // Yellow
    { 255, 255, 255, 255 }, // White
};
// clang-format on

static const GFX_VERTEX m_Vertices[4] = {
    { { 0, 0, 0 }, { 255, 255, 255, 255 }, { 0, 0 } },
    { { VIDEO_WIDTH, 0, 0 }, { 255, 255, 255, 255 }, { 1, 0 } },
    { { VIDEO_WIDTH, VIDEO_HEIGHT, 0 }, { 255, 255, 255, 255 }, { 1, 1 } },
    { { 0, VIDEO_HEIGHT, 0 }, { 255, 255, 255, 255 }, { 0, 1 } },
};

RESULT Presenter_Init(void)
{
    memset(m_Layers, 0, sizeof(m_Layers));
    memset(m_Pixels, 0, sizeof(m_Pixels));
    for (size_t i = 0; i < ARRAY_SIZE(m_Layers); i++) {
        const RESULT result =
            GfxTexture_CreateIndexed(VIDEO_WIDTH, VIDEO_HEIGHT, &m_Layers[i]);
        if (!IS_OK(result)) {
            Presenter_Close();
            return result;
        }
        m_Pixels[i] = Mem_Alloc(VIDEO_VISIBLE_SIZE);
    }
    GfxTexture_UploadPalette(m_Layers[1], 0, 9, m_TextPalette);
    return OK;
}

void Presenter_Close(void)
{
    for (size_t i = 0; i < ARRAY_SIZE(m_Layers); i++) {
        GfxTexture_Free(m_Layers[i]);
        Mem_Free(m_Pixels[i]);
    }
    memset(m_Layers, 0, sizeof(m_Layers));
    memset(m_Pixels, 0, sizeof(m_Pixels));
}

void Presenter_Draw(
    const bool text_visible, const GAIJI *const gaiji,
    const TRAM_FONT *const font, const uint16_t first_cell,
    const bool blink_visible)
{
    const GFX_RECT screen = { 0, 0, VIDEO_WIDTH, VIDEO_HEIGHT };
    const GFX_VIEW view = GfxView_Screen();
    Gfx_SetView(&view);
    Video_Scanout(m_Pixels[0], VIDEO_WIDTH);
    GfxTexture_UploadIndices(m_Layers[0], screen, m_Pixels[0]);
    GfxTexture_UploadPalette(m_Layers[0], 0, 16, g_Video.palette);
    const GFX_STATE graphics = {};
    Gfx_DrawQuad(&graphics, m_Layers[0], m_Vertices, 0);
    if (text_visible) {
        Tram_Scanout(
            gaiji, font, first_cell, blink_visible, m_Pixels[1], VIDEO_WIDTH);
        GfxTexture_UploadIndices(m_Layers[1], screen, m_Pixels[1]);
        const GFX_STATE text = { .blend = GFX_BLEND_ALPHA };
        Gfx_DrawQuad(&text, m_Layers[1], m_Vertices, 0);
    }
}
