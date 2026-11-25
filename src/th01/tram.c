#include "th01/tram.h"

#include "core/base/assert.h"

#include <string.h>

const GFX_COLOR g_TramPalette[9] = {
    { 0, 0, 0, 0 },       { 0, 0, 0, 255 },     { 0, 0, 255, 255 },
    { 255, 0, 0, 255 },   { 255, 0, 255, 255 }, { 0, 255, 0, 255 },
    { 0, 255, 255, 255 }, { 255, 255, 0, 255 }, { 255, 255, 255, 255 },
};

static TRAM m_Tram;
const TRAM *const g_Tram = &m_Tram;

static uint8_t M_Index(const TRAM_COLOR color)
{
    ASSERT(color >= TRAM_TRANSPARENT && color <= TRAM_WHITE);
    return (uint8_t)(color + 1);
}

static void M_Glyph(const GAME_TEXT_GLYPH *const glyph)
{
    ASSERT(glyph != nullptr);
    ASSERT(glyph->width == 8 || glyph->width == 16);
}

void Tram_Init(const ALLOCATOR *const allocator)
{
    m_Tram.pixels = Allocator_AllocZ(allocator, VIDEO_PAGE_SIZE);
    m_Tram.visible = true;
}

void Tram_SetVisible(const bool visible)
{
    m_Tram.visible = visible;
}

void Tram_Clear(void)
{
    memset(m_Tram.pixels, 0, VIDEO_PAGE_SIZE);
}

void Tram_Fill(const GFX_RECT rectangle, const TRAM_COLOR color)
{
    const uint8_t index = M_Index(color);
    if (rectangle.width <= 0 || rectangle.height <= 0) {
        return;
    }
    const int64_t left = rectangle.x < 0 ? 0 : rectangle.x;
    const int64_t top = rectangle.y < 0 ? 0 : rectangle.y;
    const int64_t right = (int64_t)rectangle.x + rectangle.width;
    const int64_t bottom = (int64_t)rectangle.y + rectangle.height;
    const int64_t clipped_right = right < VIDEO_WIDTH ? right : VIDEO_WIDTH;
    const int64_t clipped_bottom =
        bottom < VIDEO_HEIGHT ? bottom : VIDEO_HEIGHT;
    if (left >= clipped_right || top >= clipped_bottom) {
        return;
    }
    for (int64_t y = top; y < clipped_bottom; y++) {
        memset(
            m_Tram.pixels + y * VIDEO_WIDTH + left, index,
            (size_t)(clipped_right - left));
    }
}

static void M_PutGlyph16(
    const GAME_TEXT_GLYPH *const glyph, const int32_t left, const int32_t top,
    const TRAM_COLOR foreground, const TRAM_COLOR background)
{
    M_Glyph(glyph);
    const uint8_t foreground_index = M_Index(foreground);
    const uint8_t background_index = M_Index(background);
    for (int32_t row = 0; row < 16; row++) {
        const int64_t y0 = (int64_t)top + row * 16;
        const int64_t y1 = y0 + 16;
        if (y1 <= 0 || y0 >= VIDEO_HEIGHT) {
            continue;
        }
        for (int32_t column = 0; column < glyph->width; column++) {
            const int64_t x0 = (int64_t)left + column * 16;
            const int64_t x1 = x0 + 16;
            if (x1 <= 0 || x0 >= VIDEO_WIDTH) {
                continue;
            }
            const int64_t clipped_left = x0 < 0 ? 0 : x0;
            const int64_t clipped_right = x1 < VIDEO_WIDTH ? x1 : VIDEO_WIDTH;
            const int64_t clipped_top = y0 < 0 ? 0 : y0;
            const int64_t clipped_bottom =
                y1 < VIDEO_HEIGHT ? y1 : VIDEO_HEIGHT;
            const uint8_t index = (glyph->rows[row] & (0x8000u >> column)) != 0
                ? foreground_index
                : background_index;
            for (int64_t y = clipped_top; y < clipped_bottom; y++) {
                memset(
                    m_Tram.pixels + y * VIDEO_WIDTH + clipped_left, index,
                    (size_t)(clipped_right - clipped_left));
            }
        }
    }
}

void Tram_PutLine16(
    const GAME_TEXT_GLYPH *const *const glyphs, const int32_t count,
    const TRAM_COLOR foreground, const TRAM_COLOR background)
{
    ASSERT(count >= 0 && count <= 5);
    ASSERT(count == 0 || glyphs != nullptr);
    int32_t width = 0;
    for (int32_t i = 0; i < count; i++) {
        M_Glyph(glyphs[i]);
        width += glyphs[i]->width;
    }
    ASSERT(width <= 40);
    Tram_Fill((GFX_RECT) { 0, 0, VIDEO_WIDTH, 336 }, background);
    int32_t left = (VIDEO_WIDTH - width * 16) / 2;
    for (int32_t i = 0; i < count; i++) {
        M_PutGlyph16(glyphs[i], left, 80, foreground, background);
        left += glyphs[i]->width * 16;
    }
}

void Tram_PutKanji16Reverse(const GAME_TEXT_GLYPH *const glyph)
{
    M_Glyph(glyph);
    ASSERT(glyph->width == 16);
    Tram_Fill((GFX_RECT) { 0, 0, VIDEO_WIDTH, VIDEO_HEIGHT }, TRAM_BLACK);
    M_PutGlyph16(glyph, 192, 80, TRAM_TRANSPARENT, TRAM_BLACK);
}
