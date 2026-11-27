#include "core/text/private.h"

#include "core/base/log.h"
#include "core/base/memory.h"

#include <math.h>
#include <string.h>

static bool m_Initialized;

uint32_t Text_ReadBe(const uint8_t *const data, const int32_t bytes)
{
    uint32_t value = 0;
    for (int32_t i = 0; i < bytes; i++) {
        value = value << 8 | data[i];
    }
    return value;
}

// Returns the em size for each pixel of cell height: units per em of the
// head table over usWinAscent + usWinDescent of the OS/2 table. Returns 1
// when the data lacks either table. A collection uses its first font, as
// the font of face_index.
static float M_CellScale(
    const uint8_t *const data, const size_t size, const int32_t face_index,
    float *const ascent_scale)
{
    *ascent_scale = 0.0f;
    size_t base = 0;
    if (size >= 16 && memcmp(data, "ttcf", 4) == 0) {
        const size_t entry = 12 + (size_t)face_index * 4;
        if (face_index >= (int32_t)Text_ReadBe(data + 8, 4)
            || entry + 4 > size) {
            return 1.0f;
        }
        base = Text_ReadBe(data + entry, 4);
    }
    if (base > size || size - base < 12) {
        return 1.0f;
    }
    const uint32_t count = Text_ReadBe(data + base + 4, 2);
    uint32_t units = 0;
    uint32_t cell = 0;
    uint32_t ascent = 0;
    for (uint32_t i = 0; i < count; i++) {
        const size_t record = base + 12 + (size_t)i * 16;
        if (record + 16 > size) {
            break;
        }
        const size_t offset = Text_ReadBe(data + record + 8, 4);
        const size_t length = Text_ReadBe(data + record + 12, 4);
        if (offset > size || length > size - offset) {
            continue;
        }
        if (memcmp(data + record, "head", 4) == 0 && length >= 20) {
            units = Text_ReadBe(data + offset + 18, 2);
        } else if (memcmp(data + record, "OS/2", 4) == 0 && length >= 78) {
            ascent = Text_ReadBe(data + offset + 74, 2);
            cell = ascent + Text_ReadBe(data + offset + 76, 2);
        }
    }
    if (units != 0 && cell != 0) {
        *ascent_scale = (float)ascent / (float)units;
    }
    return units != 0 && cell != 0 ? (float)units / (float)cell : 1.0f;
}

// Keeps GDI's one-pixel advance for synthetic bold instead of SDL_ttf's
// size-dependent overhang.
static RESULT M_UpdateBoldAdvance(FONT *const font)
{
    font->bold_advance_adjust = 0;
    if (font->native_bold) {
        return OK;
    }
    const TTF_FontStyleFlags previous = TTF_GetFontStyle(font->font);
    int regular;
    int bold;
    TTF_SetFontStyle(font->font, TTF_STYLE_NORMAL);
    const bool measured_regular = TTF_GetGlyphMetrics(
        font->font, ' ', nullptr, nullptr, nullptr, nullptr, &regular);
    TTF_SetFontStyle(font->font, TTF_STYLE_BOLD);
    const bool measured_bold = TTF_GetGlyphMetrics(
        font->font, ' ', nullptr, nullptr, nullptr, nullptr, &bold);
    TTF_SetFontStyle(font->font, previous);
    FAIL_IF(
        !measured_regular || !measured_bold, "Cannot measure bold advance: %s",
        SDL_GetError());
    font->bold_advance_adjust = regular + 1 - bold;
    return OK;
}

// Sets the em size of font and its fallbacks, as GDI's font linking draws
// the linked faces at the base font's em.
static RESULT M_SetEm(FONT *const font, const float em)
{
    if (TTF_GetFontSize(font->font) != em) {
        FAIL_IF(
            !TTF_SetFontSize(font->font, em), "Cannot resize font: %s",
            SDL_GetError());
        MUST(TextRaster_SetSize(font->raster_face, em));
        MUST(M_UpdateBoldAdvance(font));
    }
    if (font->fallback != nullptr) {
        MUST(M_SetEm(font->fallback, em));
    }
    return OK;
}

RESULT Text_Init(void)
{
    if (!m_Initialized) {
        FAIL_IF(!TTF_Init(), "Cannot initialize fonts: %s", SDL_GetError());
        const RESULT raster = TextRaster_Init();
        if (!IS_OK(raster)) {
            TTF_Quit();
            return raster;
        }
        m_Initialized = true;
    }
    return OK;
}

void Text_Shutdown(void)
{
    if (m_Initialized) {
        TextRaster_Shutdown();
        TTF_Quit();
        m_Initialized = false;
    }
}

RESULT Font_Open(
    const void *const data, const size_t size, const int32_t face_index,
    const float pixel_height, FONT **const out)
{
    *out = nullptr;
    FAIL_IF(!m_Initialized, "Fonts are not initialized");
    FAIL_IF(
        data == nullptr || face_index < 0 || !isfinite(pixel_height)
            || pixel_height <= 0.0f || pixel_height > 4096.0f,
        "Invalid font data or size");
    FAIL_IF(size == 0, "Empty font data");
    SDL_IOStream *const stream = SDL_IOFromConstMem(data, size);
    if (stream == nullptr) {
        return FAIL("Cannot open font data: %s", SDL_GetError());
    }
    float ascent_scale;
    const float cell_scale = M_CellScale(data, size, face_index, &ascent_scale);
    const SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetPointerProperty(
        props, TTF_PROP_FONT_CREATE_IOSTREAM_POINTER, stream);
    SDL_SetBooleanProperty(
        props, TTF_PROP_FONT_CREATE_IOSTREAM_AUTOCLOSE_BOOLEAN, true);
    SDL_SetFloatProperty(
        props, TTF_PROP_FONT_CREATE_SIZE_FLOAT, pixel_height * cell_scale);
    SDL_SetNumberProperty(props, TTF_PROP_FONT_CREATE_FACE_NUMBER, face_index);
    TTF_Font *const face = TTF_OpenFontWithProperties(props);
    SDL_DestroyProperties(props);
    if (face == nullptr) {
        return FAIL("Cannot open font: %s", SDL_GetError());
    }
    FONT *const font = Mem_AllocZ(sizeof(*font));
    font->font = face;
    font->cell_scale = cell_scale;
    font->ascent_scale = ascent_scale;
    font->native_bold = (TTF_GetFontStyle(face) & TTF_STYLE_BOLD) != 0;
    const RESULT raster = TextRaster_Open(
        data, size, face_index, TTF_GetFontSize(face), &font->raster_face);
    if (!IS_OK(raster)) {
        Font_Free(font);
        return raster;
    }
    const RESULT metrics = M_UpdateBoldAdvance(font);
    if (!IS_OK(metrics)) {
        Font_Free(font);
        return metrics;
    }
    *out = font;
    return OK;
}

static RESULT M_SetSize(
    FONT *const font, const float pixel_height, const bool rounded)
{
    FAIL_IF(
        font == nullptr || !isfinite(pixel_height) || pixel_height <= 0.0f
            || pixel_height > 4096.0f,
        "Invalid font size");
    float size = pixel_height * font->cell_scale;
    if (rounded) {
        size = roundf(size);
        if (size < 1.0f) {
            size = 1.0f;
        }
    }
    return M_SetEm(font, size);
}

RESULT Font_SetEmToCell(FONT *const font)
{
    FAIL_IF(font == nullptr, "Missing font");
    const float pixel_height = TTF_GetFontSize(font->font) / font->cell_scale;
    font->cell_scale = 1.0f;
    return M_SetEm(font, pixel_height);
}

RESULT Font_SetSize(FONT *const font, const float pixel_height)
{
    return M_SetSize(font, pixel_height, false);
}

RESULT Font_SetSizeRounded(
    FONT *const font, const float pixel_height, int32_t *const offset_y)
{
    FAIL_IF(offset_y == nullptr, "Missing font baseline offset");
    *offset_y = 0;
    MUST(M_SetSize(font, pixel_height, true));
    if (font->ascent_scale > 0.0f) {
        *offset_y =
            (int32_t)roundf(TTF_GetFontSize(font->font) * font->ascent_scale)
            - TTF_GetFontAscent(font->font);
    }
    return OK;
}

void Font_SetFallback(FONT *const font, FONT *const fallback)
{
    if (font->fallback != nullptr) {
        TTF_RemoveFallbackFont(font->font, font->fallback->font);
        Font_Free(font->fallback);
    }
    font->fallback = fallback;
    if (fallback == nullptr) {
        return;
    }
    Result_Log(
        M_SetEm(fallback, TTF_GetFontSize(font->font)), LOG_LEVEL_WARNING);
    if (!TTF_AddFallbackFont(font->font, fallback->font)) {
        LOG_WARNING("Cannot add a fallback font: %s", SDL_GetError());
    }
}

void Font_Free(FONT *const font)
{
    if (font != nullptr) {
        TextRaster_Close(font->raster_face);
        TTF_CloseFont(font->font);
        Font_Free(font->fallback);
        Mem_Free(font);
    }
}
