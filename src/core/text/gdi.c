#include "core/text/private.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H

#include <limits.h>
#include <math.h>

// GDI's antialiased text draws outlines, never a font's embedded bitmaps.
#define M_LOAD_FLAGS (FT_LOAD_NO_BITMAP | FT_LOAD_TARGET_NORMAL)

static FT_Library m_Library;

// GDI's 17 grayscale levels of glyph coverage.
static const uint8_t m_Intensities[17] = {
    0,   77,  104, 124, 140, 154, 167, 178, 189,
    199, 208, 217, 225, 233, 240, 248, 255,
};

static uint8_t M_Coverage(
    const FT_Bitmap *const bitmap, const int32_t x, const int32_t y)
{
    if (x < 0 || x >= (int32_t)bitmap->width) {
        return 0;
    }
    const uint8_t *const row = bitmap->buffer + (ptrdiff_t)y * bitmap->pitch;
    if (bitmap->pixel_mode == FT_PIXEL_MODE_MONO) {
        return (row[x / 8] & (0x80 >> (x % 8))) != 0 ? 255 : 0;
    }
    return bitmap->num_grays > 1
        ? (uint8_t)((uint32_t)row[x] * 255 / (bitmap->num_grays - 1))
        : 0;
}

static uint8_t M_Expand5(const int32_t value)
{
    return (uint8_t)((value << 3) | (value >> 2));
}

static uint8_t M_Paint(
    const uint8_t destination, const uint8_t color, const int32_t level)
{
    const int32_t text = M_Expand5(color >> 3);
    const int32_t low = m_Intensities[level] * text / 255;
    const int32_t high = m_Intensities[16 - level]
        + (255 - m_Intensities[16 - level]) * text / 255;
    int32_t value = text;
    if (destination < text) {
        value = low + destination * (text - low) / text;
    } else if (destination > text) {
        value = high - (255 - destination) * (high - text) / (255 - text);
    }
    return M_Expand5(value >> 3);
}

RESULT TextRaster_Init(void)
{
    FAIL_IF(
        FT_Init_FreeType(&m_Library) != 0,
        "Cannot initialize glyph rasterizer");
    return OK;
}

void TextRaster_Shutdown(void)
{
    if (m_Library != nullptr) {
        FT_Done_FreeType(m_Library);
        m_Library = nullptr;
    }
}

RESULT TextRaster_SetSize(void *const handle, const float em)
{
    const uint32_t pixels = (uint32_t)fmaxf(1.0f, roundf(em));
    FAIL_IF(
        FT_Set_Pixel_Sizes(handle, 0, pixels) != 0,
        "Cannot set glyph raster size");
    return OK;
}

RESULT TextRaster_Open(
    const uint8_t *const data, const size_t size, const int32_t face_index,
    const float em, void **const out)
{
    *out = nullptr;
    FAIL_IF(size > LONG_MAX, "Font data exceeds the rasterizer limit");
    FT_Face face;
    FAIL_IF(
        FT_New_Memory_Face(m_Library, data, (FT_Long)size, face_index, &face)
            != 0,
        "Cannot open glyph rasterizer font");
    const RESULT result = TextRaster_SetSize(face, em);
    if (!IS_OK(result)) {
        FT_Done_Face(face);
        return result;
    }
    *out = face;
    return OK;
}

void TextRaster_Close(void *const face)
{
    if (face != nullptr) {
        FT_Done_Face(face);
    }
}

RESULT TextRaster_Glyph(
    const FONT *const font, const uint32_t index, SDL_Surface **const out,
    int32_t *const offset_x, int32_t *const offset_y)
{
    *out = nullptr;
    FT_Face const face = font->raster_face;
    FAIL_IF(
        FT_Load_Glyph(face, index, M_LOAD_FLAGS) != 0, "Cannot load GDI glyph");
    const int32_t left =
        (int32_t)floorf((float)face->glyph->metrics.horiBearingX / 64.0f);
    const int32_t top =
        (int32_t)ceilf((float)face->glyph->metrics.horiBearingY / 64.0f);
    const uint32_t style = TTF_GetFontStyle(font->font);
    const bool bold = (style & TTF_STYLE_BOLD) != 0
        && (face->style_flags & FT_STYLE_FLAG_BOLD) == 0;
    const bool outline = face->glyph->format == FT_GLYPH_FORMAT_OUTLINE;
    if (outline) {
        if (bold) {
            const FT_Pos strength =
                ((FT_Pos)face->size->metrics.y_ppem * 64 + 12) / 24;
            FAIL_IF(
                FT_Outline_Embolden(&face->glyph->outline, strength) != 0,
                "Cannot embolden GDI glyph");
        }
        if ((style & TTF_STYLE_ITALIC) != 0
            && (face->style_flags & FT_STYLE_FLAG_ITALIC) == 0) {
            const FT_Matrix italic = { 65536, 16384, 0, 65536 };
            FT_Outline_Transform(&face->glyph->outline, &italic);
        }
    }
    FAIL_IF(
        FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL) != 0,
        "Cannot rasterize GDI glyph");
    const FT_Bitmap *const bitmap = &face->glyph->bitmap;
    if (bitmap->width == 0 || bitmap->rows == 0) {
        return OK;
    }
    FAIL_IF(
        bitmap->pixel_mode != FT_PIXEL_MODE_MONO
            && bitmap->pixel_mode != FT_PIXEL_MODE_GRAY,
        "Unsupported GDI glyph bitmap");
    const bool extend = bold && !outline;
    SDL_Surface *const surface = SDL_CreateSurface(
        (int32_t)bitmap->width + extend, (int32_t)bitmap->rows,
        SDL_PIXELFORMAT_RGBA32);
    FAIL_IF(
        surface == nullptr, "Cannot create GDI glyph pixels: %s",
        SDL_GetError());
    for (int32_t y = 0; y < surface->h; y++) {
        for (int32_t x = 0; x < surface->w; x++) {
            int32_t coverage = M_Coverage(bitmap, x, y);
            if (extend) {
                const int32_t previous = M_Coverage(bitmap, x - 1, y);
                coverage = previous > coverage ? previous : coverage;
            }
            const int32_t level = coverage * 17 / 256;
            uint8_t *const pixel = (uint8_t *)surface->pixels
                + (size_t)y * (size_t)surface->pitch + (size_t)x * 4;
            pixel[0] = pixel[1] = pixel[2] = 255;
            pixel[3] = (uint8_t)((level * 255 + 8) / 16);
        }
    }
    *offset_x = face->glyph->bitmap_left - left;
    *offset_y = top - face->glyph->bitmap_top;
    *out = surface;
    return OK;
}

bool TextRaster_IsSimple(
    const FONT *const font, const char *const text, const size_t size)
{
    if ((TTF_GetFontStyle(font->font)
         & (TTF_STYLE_ITALIC | TTF_STYLE_UNDERLINE))
        != 0) {
        return false;
    }
    const char *cursor = text;
    size_t remaining = size;
    while (remaining != 0) {
        const uint32_t c = SDL_StepUTF8(&cursor, &remaining);
        if (!((c >= 32 && c <= 126) || (c >= 0xa0 && c <= 0x24f)
              || (c >= 0x2000 && c <= 0x200a) || (c >= 0x2010 && c <= 0x2027)
              || (c >= 0x3000 && c <= 0x3029) || (c >= 0x3030 && c <= 0x3098)
              || (c >= 0x309b && c <= 0x30ff)
              || (c >= 0x3400 && c <= 0x9fff))) {
            return false;
        }
    }
    return true;
}

static const FONT *M_CharacterFont(
    const FONT *const font, const uint32_t character)
{
    for (const FONT *f = font; f != nullptr; f = f->fallback) {
        if (FT_Get_Char_Index(f->raster_face, character) != 0) {
            return f;
        }
    }
    return font;
}

static RESULT M_Advance(
    const FONT *const font, const uint32_t index, int32_t *const advance)
{
    FT_Face const face = font->raster_face;
    FAIL_IF(
        FT_Load_Glyph(face, index, M_LOAD_FLAGS) != 0,
        "Cannot measure GDI glyph");
    const bool bold = (TTF_GetFontStyle(font->font) & TTF_STYLE_BOLD) != 0
        && (face->style_flags & FT_STYLE_FLAG_BOLD) == 0;
    *advance = (int32_t)((face->glyph->advance.x + 32) / 64) + bold;
    return OK;
}

RESULT TextRaster_MeasureSimple(
    const FONT *const font, const char *const text, const size_t size,
    int32_t *const width, int32_t *const height)
{
    int64_t pen = 0;
    const char *cursor = text;
    size_t remaining = size;
    while (remaining != 0) {
        const uint32_t character = SDL_StepUTF8(&cursor, &remaining);
        const FONT *const f = M_CharacterFont(font, character);
        int32_t advance = 0;
        MUST(M_Advance(
            f, FT_Get_Char_Index(f->raster_face, character), &advance));
        pen += advance;
        FAIL_IF(pen > INT32_MAX, "GDI text exceeds the image limit");
    }
    *width = (int32_t)pen;
    *height = TTF_GetFontHeight(font->font);
    return OK;
}

RESULT TextRaster_RenderSimple(
    const FONT *const font, const char *const text, const size_t size,
    SDL_Surface **const out)
{
    *out = nullptr;
    int32_t width, height;
    MUST(TextRaster_MeasureSimple(font, text, size, &width, &height));
    SDL_Surface *const surface =
        SDL_CreateSurface(width, height, SDL_PIXELFORMAT_RGBA32);
    FAIL_IF(
        surface == nullptr, "Cannot create GDI text pixels: %s",
        SDL_GetError());
    int32_t pen = 0;
    RESULT result = OK;
    const char *cursor = text;
    size_t remaining = size;
    while (remaining != 0) {
        const uint32_t character = SDL_StepUTF8(&cursor, &remaining);
        const FONT *const f = M_CharacterFont(font, character);
        const uint32_t index = FT_Get_Char_Index(f->raster_face, character);
        int32_t advance = 0;
        result = M_Advance(f, index, &advance);
        if (!IS_OK(result)) {
            break;
        }
        SDL_Surface *glyph;
        int32_t offset_x, offset_y;
        result = TextRaster_Glyph(f, index, &glyph, &offset_x, &offset_y);
        if (!IS_OK(result)) {
            break;
        }
        if (glyph != nullptr) {
            FT_Face const face = f->raster_face;
            const int32_t left = pen + face->glyph->bitmap_left;
            const int32_t top =
                TTF_GetFontAscent(font->font) - face->glyph->bitmap_top;
            for (int32_t y = 0; y < glyph->h; y++) {
                const int32_t yy = top + y;
                if (yy < 0 || yy >= height) {
                    continue;
                }
                for (int32_t x = 0; x < glyph->w; x++) {
                    const int32_t xx = left + x;
                    if (xx < 0 || xx >= width) {
                        continue;
                    }
                    const uint8_t alpha = ((uint8_t *)glyph->pixels)
                        [(size_t)y * glyph->pitch + (size_t)x * 4 + 3];
                    uint8_t *const pixel = (uint8_t *)surface->pixels
                        + (size_t)yy * surface->pitch + (size_t)xx * 4;
                    pixel[0] = pixel[1] = pixel[2] = 255;
                    pixel[3] = alpha > pixel[3] ? alpha : pixel[3];
                }
            }
            SDL_DestroySurface(glyph);
        }
        pen += advance;
    }
    if (!IS_OK(result)) {
        SDL_DestroySurface(surface);
        return result;
    }
    *out = surface;
    return OK;
}

uint8_t Text_GdiIntensity(const uint8_t coverage)
{
    const int32_t level = (coverage * 16 + 127) / 255;
    return level <= 1 ? 0 : m_Intensities[level];
}

void Text_StampGdi(
    IMAGE *const surface, const IMAGE *const glyphs, const int32_t x,
    const int32_t y, const GFX_COLOR color)
{
    const uint8_t rgb[3] = { color.red, color.green, color.blue };
    for (int32_t row = 0; row < glyphs->height; row++) {
        const int32_t target_y = row + y;
        if (target_y < 0 || target_y >= surface->height) {
            continue;
        }
        for (int32_t column = 0; column < glyphs->width; column++) {
            const int32_t target_x = column + x;
            const uint8_t coverage =
                glyphs->pixels
                    [((size_t)row * (size_t)glyphs->width + (size_t)column) * 4
                     + 3];
            const int32_t level = (coverage * 16 + 127) / 255;
            if (level <= 1 || target_x < 0 || target_x >= surface->width) {
                continue;
            }
            uint8_t *const pixel = surface->pixels
                + ((size_t)target_y * (size_t)surface->width + (size_t)target_x)
                    * 4;
            for (int32_t i = 0; i < 3; i++) {
                pixel[i] = M_Paint(pixel[i], rgb[i], level);
            }
            pixel[3] = 255;
        }
    }
}
