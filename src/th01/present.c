#include "th01/present.h"

#include "th01/tram.h"
#include "th01/video.h"

#include "core/gfx.h"

#include <string.h>

static GFX_TEXTURE *m_TextTexture;
static uint8_t *m_UploadedText;
static uint8_t *m_ScrolledPage;
static GFX_TEXTURE *m_Pages[2];
static uint8_t *m_UploadedPages[2];
static GFX_COLOR m_UploadedPalette[16];

RESULT Present_Init(const ALLOCATOR *const allocator)
{
    for (int32_t i = 0; i < 2; i++) {
        MUST(GfxTexture_CreateIndexed(VIDEO_WIDTH, VIDEO_HEIGHT, &m_Pages[i]));
        GfxTexture_UploadIndices(
            m_Pages[i], (GFX_RECT) { 0, 0, VIDEO_WIDTH, VIDEO_HEIGHT },
            g_Video.pages[i]);
        GfxTexture_UploadPalette(m_Pages[i], 0, 16, g_Video.palette);
        m_UploadedPages[i] = Allocator_Alloc(allocator, VIDEO_PAGE_SIZE);
        memcpy(m_UploadedPages[i], g_Video.pages[i], VIDEO_PAGE_SIZE);
    }
    memcpy(m_UploadedPalette, g_Video.palette, sizeof(m_UploadedPalette));
    MUST(GfxTexture_CreateIndexed(VIDEO_WIDTH, VIDEO_HEIGHT, &m_TextTexture));
    m_UploadedText = Allocator_AllocZ(allocator, VIDEO_PAGE_SIZE);
    m_ScrolledPage = Allocator_Alloc(allocator, VIDEO_PAGE_SIZE);
    GfxTexture_UploadIndices(
        m_TextTexture, (GFX_RECT) { 0, 0, 640, 400 }, m_UploadedText);
    GfxTexture_UploadPalette(m_TextTexture, 0, 9, g_TramPalette);
    return OK;
}

void Present_Frame(const bool text_visible)
{
    {
        for (int32_t i = 0; i < 2; i++) {
            const uint8_t *pixels = g_Video.pages[i];
            if (g_Video.scroll_top != 0) {
                for (int32_t y = 0; y < VIDEO_HEIGHT; y++) {
                    memcpy(
                        m_ScrolledPage + y * VIDEO_WIDTH,
                        pixels
                            + ((y + g_Video.scroll_top) % VIDEO_HEIGHT)
                                * VIDEO_WIDTH,
                        VIDEO_WIDTH);
                }
                pixels = m_ScrolledPage;
            }
            if (memcmp(m_UploadedPages[i], pixels, VIDEO_PAGE_SIZE) != 0) {
                GfxTexture_UploadIndices(
                    m_Pages[i], (GFX_RECT) { 0, 0, VIDEO_WIDTH, VIDEO_HEIGHT },
                    pixels);
                memcpy(m_UploadedPages[i], pixels, VIDEO_PAGE_SIZE);
            }
        }
        if (memcmp(
                m_UploadedPalette, g_Video.palette, sizeof(m_UploadedPalette))
            != 0) {
            for (int32_t i = 0; i < 2; i++) {
                GfxTexture_UploadPalette(m_Pages[i], 0, 16, g_Video.palette);
            }
            memcpy(
                m_UploadedPalette, g_Video.palette, sizeof(m_UploadedPalette));
        }
    }
    const TRAM *const text = text_visible ? g_Tram : nullptr;
    if (text != nullptr
        && memcmp(m_UploadedText, text->pixels, VIDEO_PAGE_SIZE) != 0) {
        GfxTexture_UploadIndices(
            m_TextTexture, (GFX_RECT) { 0, 0, 640, 400 }, text->pixels);
        memcpy(m_UploadedText, text->pixels, VIDEO_PAGE_SIZE);
    }
    const GFX_COLOR white = { 255, 255, 255, 255 };
    const GFX_VERTEX vertices[4] = {
        { { 0, 0, 0 }, white, { 0, 0 } },
        { { VIDEO_WIDTH, 0, 0 }, white, { 1, 0 } },
        { { VIDEO_WIDTH, VIDEO_HEIGHT, 0 }, white, { 1, 1 } },
        { { 0, VIDEO_HEIGHT, 0 }, white, { 0, 1 } },
    };
    const GFX_STATE state = {};
    Gfx_DrawQuad(&state, m_Pages[g_Video.visible_page], vertices, 0);
    if (text != nullptr && text->visible) {
        const GFX_STATE overlay = { .blend = GFX_BLEND_ALPHA };
        Gfx_DrawQuad(&overlay, m_TextTexture, vertices, 0);
    }
}

void Present_Close(void)
{
    for (int32_t i = 0; i < 2; i++) {
        GfxTexture_Free(m_Pages[i]);
        m_Pages[i] = nullptr;
        m_UploadedPages[i] = nullptr;
    }
    GfxTexture_Free(m_TextTexture);
    m_TextTexture = nullptr;
    m_UploadedText = nullptr;
    m_ScrolledPage = nullptr;
}
