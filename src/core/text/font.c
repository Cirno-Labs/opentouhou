#include "core/text/private.h"

#include "core/base/memory.h"

#include <math.h>

#include "text_font.h"

static bool m_Initialized;

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
    TTF_Font *const face = TTF_OpenFontIO(stream, true, pixel_height);
    if (face == nullptr) {
        Mem_Free(data);
        return FAIL("Cannot open font: %s", SDL_GetError());
    }
    TEXT_FONT *const font = Mem_Alloc(sizeof(*font));
    *font = (TEXT_FONT) { .font = face, .data = data };
    *out = font;
    return OK;
}

RESULT TextFont_LoadDefault(const float pixel_height, TEXT_FONT **const out)
{
    READER reader;
    Reader_OpenBuffer(&reader, m_DefaultFont, sizeof(m_DefaultFont));
    return TextFont_Load(&reader, pixel_height, out);
}

RESULT TextFont_SetSize(TEXT_FONT *const font, const float pixel_height)
{
    FAIL_IF(
        font == nullptr || !isfinite(pixel_height) || pixel_height <= 0.0f
            || pixel_height > 4096.0f,
        "Invalid font size");
    FAIL_IF(
        !TTF_SetFontSize(font->font, pixel_height), "Cannot resize font: %s",
        SDL_GetError());
    return OK;
}

void TextFont_Free(TEXT_FONT *const font)
{
    if (font != nullptr) {
        TTF_CloseFont(font->font);
        Mem_Free(font->data);
        Mem_Free(font);
    }
}
