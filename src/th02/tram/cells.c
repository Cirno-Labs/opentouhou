#include "th02/tram/cells.h"

#include "core/base/assert.h"

#include <string.h>

static TRAM m_Tram;
const TRAM *const g_Tram = &m_Tram;

static uint16_t M_Segment(const uint16_t y)
{
    return (uint16_t)(0xa000u + y * 10u);
}

static void M_Store(
    const uint16_t segment, const uint16_t offset, const uint16_t word)
{
    const uint32_t address = (uint32_t)segment * 16 + offset;
    if (address >= 0xa0000 && address < 0xa2000) {
        m_Tram.codes[(address - 0xa0000) / 2] = word;
    } else if (address >= 0xa2000 && address < 0xa4000) {
        m_Tram.attributes[(address - 0xa2000) / 2] = word;
    }
}

static uint16_t M_GaijiCode(const uint8_t glyph)
{
    return ((uint16_t)(glyph & 127) << 8) | (0x56 + (glyph >> 7));
}

void Tram_Init(void)
{
    memset(&m_Tram, 0, sizeof(m_Tram));
}

void Tram_Fill(const uint16_t code, const uint16_t attribute)
{
    for (size_t cell = 0; cell < TRAM_COLUMNS * TRAM_ROWS; cell++) {
        m_Tram.codes[cell] = code;
        m_Tram.attributes[cell] = attribute;
    }
}

void Tram_PutGaiji(
    const uint16_t x, const uint16_t y, const uint16_t glyph,
    const uint16_t attribute)
{
    const uint16_t segment = M_Segment(y);
    const uint16_t offset = (uint16_t)(x * 2u);
    const uint16_t code = M_GaijiCode((uint8_t)glyph);
    M_Store(segment, (uint16_t)(offset + 0x2000), attribute);
    M_Store(segment, offset, code);
    M_Store(segment, (uint16_t)(offset + 0x2002), attribute);
    M_Store(segment, (uint16_t)(offset + 2), code | 0x8000);
}

void Tram_PutGaijiString(
    const uint16_t x, const uint16_t y, const uint8_t *glyphs,
    const uint16_t attribute)
{
    const uint16_t segment = M_Segment(y);
    const uint16_t first = (uint16_t)(x * 2u);
    uint16_t offset = first;
    while (*glyphs != 0) {
        const uint16_t code = M_GaijiCode(*glyphs++);
        M_Store(segment, offset, code);
        offset = (uint16_t)(offset + 2);
        M_Store(segment, offset, code | 0x8000);
        offset = (uint16_t)(offset + 2);
    }
    const size_t count = (uint16_t)(offset - first) / 2;
    offset = (uint16_t)(first + 0x2000);
    for (size_t cell = 0; cell < count; cell++) {
        M_Store(segment, offset, attribute);
        offset = (uint16_t)(offset + 2);
    }
}

void Tram_FillAttributes(
    const uint16_t x1, const uint16_t y1, const uint16_t x2, const uint16_t y2,
    const uint16_t attribute)
{
    ASSERT(x1 <= x2 && x2 < TRAM_COLUMNS);
    ASSERT(y1 <= y2 && y2 < TRAM_ROWS);
    for (size_t y = y1; y <= y2; y++) {
        for (size_t x = x1; x <= x2; x++) {
            m_Tram.attributes[y * TRAM_COLUMNS + x] = attribute;
        }
    }
}

void Tram_PutText(
    const uint16_t x, const uint16_t y, const TRAM_TEXT *const text,
    const uint16_t source_bytes, const uint16_t attribute)
{
    const uint16_t segment = M_Segment(y);
    const uint16_t first = (uint16_t)(x * 2u);
    uint16_t offset = first;
    uint16_t previous_end = 0;
    for (int32_t i = 0; i < text->count && source_bytes > previous_end; i++) {
        const TRAM_CHARACTER *const character = &text->characters[i];
        const bool partial = source_bytes < character->source_end;
        const uint16_t code =
            partial ? character->partial_code : character->code;
        M_Store(segment, offset, code);
        offset = (uint16_t)(offset + 2);
        if (character->wide) {
            M_Store(segment, offset, code | 0x80);
            offset = (uint16_t)(offset + 2);
        }
        if (partial) {
            break;
        }
        previous_end = character->source_end;
    }
    const size_t count = (uint16_t)(offset - first) / 2;
    offset = (uint16_t)(first + 0x2000);
    for (size_t cell = 0; cell < count; cell++) {
        M_Store(segment, offset, attribute);
        offset = (uint16_t)(offset + 2);
    }
}

void Tram_PutCharacter(
    const uint16_t x, const uint16_t y, const TRAM_CHARACTER *const character,
    const uint16_t attribute)
{
    const uint16_t segment = M_Segment(y);
    const uint16_t offset = (uint16_t)(x * 2u);
    const uint16_t code =
        character->wide ? character->single_code : character->code;
    M_Store(segment, (uint16_t)(offset + 0x2000), attribute);
    M_Store(segment, offset, code);
    if (character->wide) {
        M_Store(segment, (uint16_t)(offset + 0x2002), attribute);
        M_Store(segment, (uint16_t)(offset + 2), code | 0x80);
    }
}
