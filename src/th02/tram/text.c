#include "th02/tram/text.h"

#include "core/base/array.h"
#include "core/base/cp932.h"

#include <string.h>

typedef ARRAY(TRAM_CHARACTER) CHARACTER_ARRAY;

static uint16_t M_Code(const uint8_t lead, const uint8_t trail)
{
    uint16_t value = ((uint16_t)(uint8_t)(lead * 2u) << 8) | trail;
    bool carry = trail < 0x9f;
    if (carry) {
        const uint32_t sum = value + 0xfedfu + (trail < 0x80 ? 1u : 0u);
        value = (uint16_t)sum;
        carry = sum > UINT16_MAX;
    }
    const uint8_t low = (uint8_t)((value & 255) - 0xfe - (carry ? 1 : 0));
    value = ((value & 0xff00) | low) & 0x7f7f;
    return (uint16_t)((value >> 8) | (value << 8));
}

static uint16_t M_SingleCode(const uint8_t lead, const uint8_t trail)
{
    uint16_t value = ((uint16_t)(uint8_t)(lead * 2u) << 8) | trail;
    bool carry = trail < 0x9f;
    if (carry) {
        const uint32_t sum = value + 0xfedfu + (trail < 0x80 ? 1u : 0u);
        value = (uint16_t)sum;
        carry = sum > UINT16_MAX;
    }
    value = (uint16_t)(value - 0xdffe - (carry ? 1 : 0)) & 0x7f7f;
    value = (uint16_t)((value >> 8) | (value << 8));
    return (value & 0xff00) | (uint8_t)((value & 255) - 0x20);
}

static RESULT M_Characters(
    const uint8_t *const bytes, const size_t size, const char *const utf8,
    CHARACTER_ARRAY *const characters)
{
    uint32_t text_offset = 0;
    for (size_t offset = 0; offset < size;) {
        const uint8_t first = bytes[offset];
        const bool wide = (first > 0x80 && first <= 0x9f) || first >= 0xe0;
        FAIL_IF(
            wide && (first > 0xfc || offset + 1 >= size),
            "Unsupported or truncated original text character");
        const size_t length = wide ? 2 : 1;
        const TRAM_CHARACTER character = {
            .code = wide ? M_Code(first, bytes[offset + 1]) : first,
            .partial_code = wide ? M_Code(first, 0) : first,
            .source_end = (uint16_t)(offset + length),
            .single_code =
                wide ? M_SingleCode(first, bytes[offset + 1]) : first,
            .text_offset = text_offset,
            .wide = wide,
        };
        Array_Append(characters, character);
        const uint8_t utf8_first = (uint8_t)utf8[text_offset];
        text_offset += utf8_first < 0x80 ? 1 : utf8_first < 0xe0 ? 2 : 3;
        offset += length;
    }
    return OK;
}

RESULT TramText_Read(
    READER *const reader, const size_t byte_size,
    const ALLOCATOR *const allocator, TRAM_TEXT *const out)
{
    *out = (TRAM_TEXT) {};
    MUST(Reader_Result(reader));
    FAIL_IF(
        byte_size > UINT16_MAX || byte_size > Reader_Remaining(reader),
        "Invalid original text field size");
    uint8_t *const bytes = Mem_Alloc(byte_size + 1);
    Reader_ReadBytes(reader, bytes, byte_size);
    bytes[byte_size] = 0;
    RESULT result = Reader_Result(reader);
    char *utf8 = nullptr;
    const size_t size = strlen((const char *)bytes);
    if (IS_OK(result)) {
        result = Cp932_Decode((const char *)bytes, size, &utf8);
    }
    CHARACTER_ARRAY characters;
    Array_Init(&characters, &g_HeapAllocator);
    if (IS_OK(result)) {
        result = M_Characters(bytes, size, utf8, &characters);
    }
    if (IS_OK(result)) {
        const size_t text_size = strlen(utf8) + 1;
        char *const copied_text = Allocator_Alloc(allocator, text_size);
        memcpy(copied_text, utf8, text_size);
        TRAM_CHARACTER *copied_characters = nullptr;
        if (characters.count != 0) {
            const size_t character_size = Mem_ArrSize(
                (size_t)characters.count, sizeof(*characters.items));
            copied_characters = Allocator_Alloc(allocator, character_size);
            memcpy(copied_characters, characters.items, character_size);
        }
        *out = (TRAM_TEXT) { copied_text, copied_characters, characters.count,
                             (uint16_t)size };
    }
    Array_Free(&characters);
    Mem_Free(utf8);
    Mem_Free(bytes);
    return result;
}
