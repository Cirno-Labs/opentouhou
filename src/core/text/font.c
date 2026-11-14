#include "core/text/private.h"

#include "core/base/log.h"
#include "core/base/memory.h"
#include "core/base/string.h"

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
// when the data lacks either table. A collection uses its first font, as
// the font opens it.
static float M_CellScale(const uint8_t *const data, const size_t size)
{
    size_t base = 0;
    if (size >= 16 && memcmp(data, "ttcf", 4) == 0) {
        base = M_ReadBe(data + 12, 4);
    }
    if (base > size || size - base < 12) {
        return 1.0f;
    }
    const uint32_t count = M_ReadBe(data + base + 4, 2);
    uint32_t units = 0;
    uint32_t cell = 0;
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
            cell = M_ReadBe(data + offset + 74, 2)
                + M_ReadBe(data + offset + 76, 2);
        }
    }
    return units != 0 && cell != 0 ? (float)units / (float)cell : 1.0f;
}

RESULT Text_Init(void)
{
    if (!m_Initialized) {
        FAIL_IF(!TTF_Init(), "Cannot initialize fonts: %s", SDL_GetError());
        m_Initialized = true;
    }
    return OK;
}

void Text_Shutdown(void)
{
    if (m_Initialized) {
        TTF_Quit();
        m_Initialized = false;
    }
}

RESULT TextFont_Load(
    READER *const reader, const float pixel_height, TEXT_FONT **const out)
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
    SDL_IOStream *const stream = SDL_IOFromConstMem(data, size);
    if (stream == nullptr) {
        Mem_Free(data);
        return FAIL("Cannot open font data: %s", SDL_GetError());
    }
    const float cell_scale = M_CellScale(data, size);
    TTF_Font *const face =
        TTF_OpenFontIO(stream, true, pixel_height * cell_scale);
    if (face == nullptr) {
        Mem_Free(data);
        return FAIL("Cannot open font: %s", SDL_GetError());
    }
    TEXT_FONT *const font = Mem_Alloc(sizeof(*font));
    *font =
        (TEXT_FONT) { .font = face, .data = data, .cell_scale = cell_scale };
    *out = font;
    return OK;
}

static RESULT M_LoadFile(
    const char *const path, const float pixel_height, TEXT_FONT **const out)
{
    READER reader;
    MUST(Reader_OpenFile(&reader, path));
    const RESULT result = TextFont_Load(&reader, pixel_height, out);
    Reader_Close(&reader);
    MUST(result, "%s", path);
    return OK;
}

RESULT TextFont_LoadDefault(const float pixel_height, TEXT_FONT **const out)
{
    READER reader;
    Reader_OpenBuffer(&reader, m_DefaultFont, sizeof(m_DefaultFont), nullptr);
    return TextFont_Load(&reader, pixel_height, out);
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
                M_LoadFile(paths[i], pixel_height, &font), LOG_LEVEL_WARNING)) {
            continue;
        }
        const char *const name = TTF_GetFontFamilyName(font->font);
        if (name == nullptr || !Str_EqualIgnoreCase(name, family)) {
            TextFont_Free(font);
            continue;
        }
        const RESULT fallback =
            TextFont_LoadDefault(pixel_height, &font->fallback);
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

RESULT TextFont_SetSize(TEXT_FONT *const font, const float pixel_height)
{
    FAIL_IF(
        font == nullptr || !isfinite(pixel_height) || pixel_height <= 0.0f
            || pixel_height > 4096.0f,
        "Invalid font size");
    FAIL_IF(
        !TTF_SetFontSize(font->font, pixel_height * font->cell_scale),
        "Cannot resize font: %s", SDL_GetError());
    if (font->fallback != nullptr) {
        MUST(TextFont_SetSize(font->fallback, pixel_height));
    }
    return OK;
}

void TextFont_Free(TEXT_FONT *const font)
{
    if (font != nullptr) {
        TTF_CloseFont(font->font);
        TextFont_Free(font->fallback);
        Mem_Free(font->data);
        Mem_Free(font);
    }
}
