#include "core/text/private.h"

#include "core/base/log.h"
#include "core/base/memory.h"

#include <math.h>
#include <string.h>

#include "text_font.h"

static bool m_Initialized;

static uint32_t M_ReadBe(const uint8_t *const data, const int32_t bytes)
{
    uint32_t value = 0;
    for (int32_t i = 0; i < bytes; i++) {
        value = value << 8 | data[i];
    }
    return value;
}

// Returns the em size for each pixel of cell height: units per em of the
// head table over usWinAscent + usWinDescent of the OS/2 table. Returns 1
// when the data lacks either table. Uses the selected collection face.
static float M_CellScale(
    const uint8_t *const data, const size_t size, const int32_t face_index,
    float *const ascent_scale)
{
    *ascent_scale = 0.0f;
    size_t base = 0;
    if (size >= 16 && memcmp(data, "ttcf", 4) == 0) {
        const uint32_t count = M_ReadBe(data + 8, 4);
        if ((uint32_t)face_index >= count
            || (size_t)face_index >= (size - 12) / 4) {
            return 1.0f;
        }
        base = M_ReadBe(data + 12 + (size_t)face_index * 4, 4);
    }
    if (base > size || size - base < 12) {
        return 1.0f;
    }
    const uint32_t count = M_ReadBe(data + base + 4, 2);
    uint32_t units = 0;
    uint32_t cell = 0;
    uint32_t ascent = 0;
    for (uint32_t i = 0; i < count; i++) {
        const size_t record = base + 12 + (size_t)i * 16;
        if (record + 16 > size) {
            break;
        }
        const size_t offset = M_ReadBe(data + record + 8, 4);
        const size_t length = M_ReadBe(data + record + 12, 4);
        if (offset > size || length > size - offset) {
            continue;
        }
        if (memcmp(data + record, "head", 4) == 0 && length >= 20) {
            units = M_ReadBe(data + offset + 18, 2);
        } else if (memcmp(data + record, "OS/2", 4) == 0 && length >= 78) {
            ascent = M_ReadBe(data + offset + 74, 2);
            cell = ascent + M_ReadBe(data + offset + 76, 2);
        }
    }
    if (units != 0 && cell != 0) {
        *ascent_scale = (float)ascent / (float)units;
    }
    return units != 0 && cell != 0 ? (float)units / (float)cell : 1.0f;
}

// Keeps GDI's one-pixel advance for synthetic bold instead of SDL_ttf's
// size-dependent overhang.
static RESULT M_UpdateBoldAdvance(TEXT_FONT *const font)
{
    font->bold_advance_adjust = 0;
    if (font->native_bold) {
        return OK;
    }
    const TTF_FontStyleFlags previous = TTF_GetFontStyle(font->font);
    int regular, bold;
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

static RESULT M_Load(
    READER *const reader, const float pixel_height, const char *const family,
    TEXT_FONT **const out)
{
    *out = nullptr;
    FAIL_IF(!m_Initialized, "Fonts are not initialized");
    FAIL_IF(
        reader == nullptr || !isfinite(pixel_height) || pixel_height <= 0.0f
            || pixel_height > 4096.0f,
        "Invalid font data or size");
    const size_t size = Reader_Remaining(reader);
    FAIL_IF(size == 0, "Empty font data");
    uint8_t *const data = Reader_ReadAlloc(reader, size, &g_HeapAllocator);
    const RESULT read = Reader_Result(reader);
    if (!IS_OK(read)) {
        Mem_Free(data);
        return read;
    }
    int32_t face_index = 0;
    if (family != nullptr) {
        const RESULT found =
            TextRaster_FindFace(data, size, family, &face_index);
        if (!IS_OK(found) || face_index < 0) {
            Mem_Free(data);
            return found;
        }
    }
    SDL_IOStream *const stream = SDL_IOFromConstMem(data, size);
    if (stream == nullptr) {
        Mem_Free(data);
        return FAIL("Cannot open font data: %s", SDL_GetError());
    }
    float ascent_scale;
    const float cell_scale = M_CellScale(data, size, face_index, &ascent_scale);
    const SDL_PropertiesID props = SDL_CreateProperties();
    const bool configured =
        props != 0
        && SDL_SetPointerProperty(
            props, TTF_PROP_FONT_CREATE_IOSTREAM_POINTER, stream)
        && SDL_SetBooleanProperty(
            props, TTF_PROP_FONT_CREATE_IOSTREAM_AUTOCLOSE_BOOLEAN, true)
        && SDL_SetFloatProperty(
            props, TTF_PROP_FONT_CREATE_SIZE_FLOAT, pixel_height * cell_scale)
        && SDL_SetNumberProperty(
            props, TTF_PROP_FONT_CREATE_FACE_NUMBER, face_index);
    TTF_Font *const face =
        configured ? TTF_OpenFontWithProperties(props) : nullptr;
    SDL_DestroyProperties(props);
    if (!configured) {
        SDL_CloseIO(stream);
    }
    if (face == nullptr) {
        Mem_Free(data);
        return FAIL("Cannot open font: %s", SDL_GetError());
    }
    TEXT_FONT *const font = Mem_Alloc(sizeof(*font));
    *font = (TEXT_FONT) {
        .font = face,
        .data = data,
        .cell_scale = cell_scale,
        .ascent_scale = ascent_scale,
        .native_bold = (TTF_GetFontStyle(face) & TTF_STYLE_BOLD) != 0,
    };
    const RESULT raster = TextRaster_Open(
        data, size, face_index, TTF_GetFontSize(face), &font->raster_face);
    if (!IS_OK(raster)) {
        TextFont_Free(font);
        return raster;
    }
    const RESULT metrics = M_UpdateBoldAdvance(font);
    if (!IS_OK(metrics)) {
        TextFont_Free(font);
        return metrics;
    }
    *out = font;
    return OK;
}

static RESULT M_LoadFile(
    const char *const path, const float pixel_height, const char *const family,
    TEXT_FONT **const out)
{
    READER reader;
    MUST(Reader_OpenFile(&reader, path));
    const RESULT result = M_Load(&reader, pixel_height, family, out);
    Reader_Close(&reader);
    MUST(result, "%s", path);
    return OK;
}

static RESULT M_LoadDefault(
    const float pixel_height, const char *const family, TEXT_FONT **const out)
{
    READER reader;
    Reader_OpenBuffer(&reader, m_DefaultFont, sizeof(m_DefaultFont), nullptr);
    MUST(M_Load(&reader, pixel_height, family, out));
    FAIL_IF(*out == nullptr, "Bundled font family is unavailable");
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

RESULT TextFont_Load(
    READER *const reader, const float pixel_height, TEXT_FONT **const out)
{
    return M_Load(reader, pixel_height, nullptr, out);
}

RESULT TextFont_LoadDefault(const float pixel_height, TEXT_FONT **const out)
{
    return M_LoadDefault(pixel_height, nullptr, out);
}

RESULT TextFont_LoadFamily(
    const char *const *const paths, const int32_t count,
    const char *const family, const float pixel_height, TEXT_FONT **const out)
{
    *out = nullptr;
    FAIL_IF(family == nullptr || count < 0, "Invalid font family");
    for (int32_t i = 0; i < count; i++) {
        TEXT_FONT *font;
        if (!Result_Log(
                M_LoadFile(paths[i], pixel_height, family, &font),
                LOG_LEVEL_WARNING)) {
            continue;
        }
        if (font == nullptr) {
            continue;
        }
        const RESULT fallback =
            M_LoadDefault(pixel_height, "MS UI Gothic", &font->fallback);
        if (!IS_OK(fallback)) {
            TextFont_Free(font);
            return fallback;
        }
        if (!TTF_AddFallbackFont(font->font, font->fallback->font)) {
            const RESULT result =
                FAIL("Cannot add a fallback font: %s", SDL_GetError());
            TextFont_Free(font);
            return result;
        }
        *out = font;
        return OK;
    }
    return FAIL("No font file has the family %s", family);
}

static RESULT M_SetSize(
    TEXT_FONT *const font, const float pixel_height, const bool rounded)
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
    if (TTF_GetFontSize(font->font) != size) {
        FAIL_IF(
            !TTF_SetFontSize(font->font, size), "Cannot resize font: %s",
            SDL_GetError());
        MUST(TextRaster_SetSize(font->raster_face, size));
        MUST(M_UpdateBoldAdvance(font));
    }
    if (font->fallback != nullptr) {
        MUST(M_SetSize(font->fallback, pixel_height, rounded));
    }
    return OK;
}

RESULT TextFont_SetSize(TEXT_FONT *const font, const float pixel_height)
{
    return M_SetSize(font, pixel_height, false);
}

RESULT TextFont_SetSizeRounded(
    TEXT_FONT *const font, const float pixel_height, int32_t *const offset_y)
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

void TextFont_Free(TEXT_FONT *const font)
{
    if (font != nullptr) {
        TextRaster_Close(font->raster_face);
        TTF_CloseFont(font->font);
        TextFont_Free(font->fallback);
        Mem_Free(font->data);
        Mem_Free(font);
    }
}
