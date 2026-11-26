#include "th02/draw/text.h"

#include <string.h>

typedef struct {
    uint16_t rows[16];
    bool wide;
} CACHED_GLYPH;

static uint16_t M_Jis(const uint8_t lead, const uint8_t trail)
{
    if (!((lead >= 0x81 && lead < 0xa0) || (lead >= 0xe0 && lead < 0xf0))
        || trail < 0x40 || trail >= 0xfd || trail == 0x7f) {
        return 0;
    }
    uint16_t row = (lead - (lead < 0xa0 ? 0x81 : 0xc1)) * 2 + 0x21;
    uint16_t column;
    if (trail >= 0x9f) {
        row++;
        column = trail - 0x7e;
    } else {
        column = trail - (trail < 0x7f ? 0x1f : 0x20);
    }
    return (row << 8) | column;
}

static RESULT M_Resolve(
    const uint8_t *const bytes, const TRAM_TEXT *const cells,
    const DRAW_TEXT_FONT *const font, CACHED_GLYPH *const cached)
{
    FAIL_IF(
        cells->count != 0 && (font == nullptr || font->load == nullptr),
        "Missing graphics text font");
    const size_t text_size = strlen(cells->text);
    size_t previous = 0;
    for (int32_t i = 0; i < cells->count; i++) {
        const TRAM_CHARACTER *const cell = &cells->characters[i];
        const uint8_t first = bytes[previous];
        uint16_t jis;
        if (cell->wide) {
            jis = M_Jis(first, bytes[previous + 1]);
        } else if (first >= 0xa1 && first <= 0xdf) {
            jis = first + 0x2980;
        } else if ((first >= 0x21 && first <= 0x7e) || first == 0xa0) {
            jis = first + 0x2900;
        } else {
            jis = 0x2b21;
        }
        CACHED_GLYPH *const glyph = &cached[i];
        glyph->wide = !(jis >= 0x2921 && jis <= 0x2b7e);
        const size_t end = i + 1 < cells->count
            ? cells->characters[i + 1].text_offset
            : text_size;
        const DRAW_TEXT_CHARACTER character = {
            .selector =
                (uint16_t)(((uint8_t)((jis >> 8) - 0x20) << 8) | (jis & 255)),
            .wide = glyph->wide,
            .text = cells->text + cell->text_offset,
            .size = end - cell->text_offset,
        };
        MUST(font->load(font->context, &character, glyph->rows));
        if (!glyph->wide) {
            for (size_t y = 0; y < 16; y++) {
                glyph->rows[y] &= 0xff00;
            }
        }
        previous = cell->source_end;
    }
    return OK;
}

static void M_Copy(
    const TRAM_TEXT *const cells, const CACHED_GLYPH *const cached,
    const ALLOCATOR *const allocator, DRAW_TEXT *const out)
{
    const size_t text_size = strlen(cells->text) + 1;
    char *const text = Allocator_Alloc(allocator, text_size);
    memcpy(text, cells->text, text_size);
    const size_t count = (size_t)cells->count;
    TRAM_CHARACTER *characters = nullptr;
    DRAW_TEXT_GLYPH *glyphs = nullptr;
    if (count != 0) {
        const size_t character_size = Mem_ArrSize(count, sizeof(*characters));
        characters = Allocator_Alloc(allocator, character_size);
        memcpy(characters, cells->characters, character_size);
        const size_t descriptor_size = Mem_ArrSize(count, sizeof(*glyphs));
        glyphs = Allocator_Alloc(
            allocator,
            descriptor_size + Mem_ArrSize(count, 16 * sizeof(uint16_t)));
        uint16_t *const rows =
            (uint16_t *)((uint8_t *)glyphs + descriptor_size);
        for (size_t i = 0; i < count; i++) {
            memcpy(&rows[i * 16], cached[i].rows, sizeof(cached[i].rows));
            glyphs[i] = (DRAW_TEXT_GLYPH) { &rows[i * 16], cached[i].wide };
        }
    }
    *out = (DRAW_TEXT) { { text, characters, cells->count, cells->source_size },
                         glyphs,
                         cells->count };
}

RESULT DrawText_Read(
    READER *const reader, const size_t byte_size,
    const DRAW_TEXT_FONT *const font, const ALLOCATOR *const allocator,
    DRAW_TEXT *const out)
{
    *out = (DRAW_TEXT) {};
    MUST(Reader_Result(reader));
    FAIL_IF(
        byte_size > UINT16_MAX || byte_size > Reader_Remaining(reader),
        "Invalid original graphics text field size");
    uint8_t *const bytes = Mem_Alloc(byte_size);
    Reader_ReadBytes(reader, bytes, byte_size);
    READER buffer;
    Reader_OpenBuffer(&buffer, bytes, byte_size, nullptr);
    TRAM_TEXT cells = {};
    RESULT result = Reader_Result(reader);
    if (IS_OK(result)) {
        result = TramText_Read(&buffer, byte_size, &g_HeapAllocator, &cells);
    }
    Reader_Close(&buffer);
    CACHED_GLYPH *cached = nullptr;
    if (IS_OK(result)) {
        cached = Mem_Alloc(Mem_ArrSize((size_t)cells.count, sizeof(*cached)));
        result = M_Resolve(bytes, &cells, font, cached);
    }
    if (IS_OK(result)) {
        M_Copy(&cells, cached, allocator, out);
    }
    Mem_Free(cached);
    Mem_Free((void *)cells.characters);
    Mem_Free((void *)cells.text);
    Mem_Free(bytes);
    return result;
}
