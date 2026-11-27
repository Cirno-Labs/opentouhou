#include "tests/test.h"
#include "tests/th02/th02_tram_fixture.h"

#include "core/base/arena.h"

static RESULT M_Read(
    const uint8_t *const bytes, const size_t size, ARENA *const arena,
    TRAM_TEXT *const text)
{
    READER reader;
    Reader_OpenBuffer(&reader, bytes, size, nullptr);
    const RESULT result =
        TramText_Read(&reader, size, Arena_Allocator(arena), text);
    Reader_Close(&reader);
    return result;
}

TEST(original_text_keeps_unicode_and_source_byte_reveal_boundaries)
{
    ARENA arena;
    Arena_Init(&arena, "original text", 8192);
    uint8_t bytes[] = { 'A', 0x82, 0xa0, 0xb1, 0 };
    TRAM_TEXT text;
    CHECK(Result_Ignore(M_Read(bytes, sizeof(bytes), &arena, &text)));
    CHECK(strcmp(text.text, "Aあｱ") == 0);
    CHECK_EQ_INT(text.count, 3);
    CHECK_EQ_INT(text.source_size, 4);
    CHECK_EQ_INT(text.characters[0].code, 'A');
    CHECK(!text.characters[0].wide);
    CHECK_EQ_INT(text.characters[0].source_end, 1);
    CHECK_EQ_INT(text.characters[1].code, 0x2204);
    CHECK_EQ_INT(text.characters[1].partial_code, 0x6102);
    CHECK(text.characters[1].wide);
    CHECK_EQ_INT(text.characters[1].source_end, 3);
    CHECK_EQ_INT(text.characters[1].text_offset, 1);
    CHECK_EQ_INT(text.characters[2].code, 0xb1);
    CHECK_EQ_INT(text.characters[2].text_offset, 4);
    memset(bytes, 0, sizeof(bytes));
    CHECK(strcmp(text.text, "Aあｱ") == 0);

    Tram_Init();
    const size_t used = arena.used;
    Tram_PutText(79, 0, &text, 2, 0xabe1);
    CHECK_EQ_INT(m_Tram.codes[79], 'A');
    CHECK_EQ_INT(m_Tram.codes[80], 0x6102);
    CHECK_EQ_INT(m_Tram.codes[81], 0x6182);
    CHECK_EQ_INT(m_Tram.codes[82], 0);
    CHECK_EQ_INT(m_Tram.attributes[81], 0xabe1);
    Tram_PutText(79, 0, &text, UINT16_MAX, 0x21);
    CHECK_EQ_INT(m_Tram.codes[80], 0x2204);
    CHECK_EQ_INT(m_Tram.codes[81], 0x2284);
    CHECK_EQ_INT(m_Tram.codes[82], 0xb1);
    CHECK_EQ_INT(m_Tram.attributes[82], 0x21);
    const TRAM before = m_Tram;
    Tram_PutText(0, 0, &text, 0, 0);
    CHECK(memcmp(&m_Tram, &before, sizeof(m_Tram)) == 0);
    CHECK_EQ_INT(arena.used, used);
    Arena_Free(&arena);
}

TEST(noncanonical_original_selectors_are_kept_after_unicode_decoding)
{
    ARENA arena;
    Arena_Init(&arena, "text aliases", 4096);
    const uint8_t bytes[] = { 0x87, 0x90, 0x81, 0xe0 };
    TRAM_TEXT text;
    CHECK(Result_Ignore(M_Read(bytes, sizeof(bytes), &arena, &text)));
    CHECK(strcmp(text.text, "≒≒") == 0);
    CHECK_EQ_INT(text.count, 2);
    CHECK(text.characters[0].code != text.characters[1].code);
    CHECK_EQ_INT(text.characters[0].source_end, 2);
    CHECK_EQ_INT(text.characters[1].source_end, 4);
    Arena_Free(&arena);
}

TEST(field_boundaries_and_zero_termination_do_not_read_following_bytes)
{
    ARENA arena;
    Arena_Init(&arena, "bounded text", 4096);
    const uint8_t bytes[] = { 'A', 0, 0xff, 'B' };
    TRAM_TEXT text;
    READER reader;
    Reader_OpenBuffer(&reader, bytes, sizeof(bytes), nullptr);
    CHECK(Result_Ignore(
        TramText_Read(&reader, 3, Arena_Allocator(&arena), &text)));
    CHECK(strcmp(text.text, "A") == 0);
    CHECK_EQ_INT(text.count, 1);
    CHECK_EQ_INT(Reader_Tell(&reader), 3);
    CHECK_EQ_INT(Reader_ReadU8(&reader), 'B');
    Reader_Close(&reader);
    CHECK(Result_Ignore(M_Read(nullptr, 0, &arena, &text)));
    CHECK(strcmp(text.text, "") == 0);
    CHECK(text.characters == nullptr && text.count == 0);
    Arena_Free(&arena);
}

TEST(invalid_original_text_leaves_no_final_allocations)
{
    ARENA arena;
    Arena_Init(&arena, "invalid original text", 4096);
    const uint8_t truncated[] = { 0x82 };
    const uint8_t invalid[] = { 0x82, 0x20 };
    const uint8_t extension[] = { 0xfd, 'A' };
    TRAM_TEXT text;
    CHECK(!Result_Ignore(M_Read(truncated, sizeof(truncated), &arena, &text)));
    CHECK(!Result_Ignore(M_Read(invalid, sizeof(invalid), &arena, &text)));
    CHECK(!Result_Ignore(M_Read(extension, sizeof(extension), &arena, &text)));
    CHECK(
        text.text == nullptr && text.characters == nullptr && text.count == 0);
    CHECK_EQ_INT(arena.used, 0);
    READER reader;
    Reader_OpenBuffer(&reader, nullptr, 0, nullptr);
    CHECK(!Result_Ignore(
        TramText_Read(&reader, 1, Arena_Allocator(&arena), &text)));
    CHECK(!Result_Ignore(
        TramText_Read(&reader, 65536, Arena_Allocator(&arena), &text)));
    Reader_Close(&reader);
    CHECK_EQ_INT(arena.used, 0);
    Arena_Free(&arena);
}

TEST(text_writes_keep_the_code_then_attribute_order_at_plane_boundaries)
{
    ARENA arena;
    Arena_Init(&arena, "overlapping text", 4096);
    const uint8_t bytes[] = { 'A', 0x82, 0xa0, 'B' };
    TRAM_TEXT text;
    CHECK(Result_Ignore(M_Read(bytes, sizeof(bytes), &arena, &text)));

    memset(&m_Tram, 0xa5, sizeof(m_Tram));
    Tram_PutText(4095, 0, &text, 2, 0x4321);
    CHECK_EQ_INT(m_Tram.codes[4095], 'A');
    CHECK_EQ_INT(m_Tram.attributes[0], 0x6102);
    CHECK_EQ_INT(m_Tram.attributes[1], 0x6182);
    CHECK_EQ_INT(m_Tram.attributes[4095], 0x4321);
    Tram_Init();
    Tram_PutText(32767, 0, &text, UINT16_MAX, 0x21);
    CHECK_EQ_INT(m_Tram.codes[0], 0x2204);
    CHECK_EQ_INT(m_Tram.codes[1], 0x2284);
    CHECK_EQ_INT(m_Tram.codes[2], 'B');
    CHECK_EQ_INT(m_Tram.attributes[0], 0x21);
    CHECK_EQ_INT(m_Tram.attributes[2], 0x21);
    Arena_Free(&arena);
}

TEST(single_character_writes_keep_ank_literals_and_wide_cell_attributes)
{
    memset(&m_Tram, 0xa5, sizeof(m_Tram));
    const TRAM_CHARACTER space = { .code = ' ' };
    Tram_PutCharacter(39, 24, &space, 0xe1);
    CHECK_EQ_INT(m_Tram.codes[24 * 80 + 39], ' ');
    CHECK_EQ_INT(m_Tram.attributes[24 * 80 + 39], 0xe1);
    CHECK_EQ_INT(m_Tram.codes[24 * 80 + 40], 0xa5a5);
    const TRAM_CHARACTER nul = {};
    Tram_PutCharacter(0, 0, &nul, 0x21);
    CHECK_EQ_INT(m_Tram.codes[0], 0);
    CHECK_EQ_INT(m_Tram.attributes[0], 0x21);
    CHECK_EQ_INT(m_Tram.codes[1], 0xa5a5);
    ARENA arena;
    Arena_Init(&arena, "single imported character", 4096);
    const uint8_t bytes[] = { 0x82, 0xa0 };
    TRAM_TEXT text;
    CHECK(Result_Ignore(M_Read(bytes, sizeof(bytes), &arena, &text)));
    Tram_PutCharacter(79, 0, &text.characters[0], 0x43e1);
    CHECK_EQ_INT(m_Tram.codes[79], 0x2204);
    CHECK_EQ_INT(m_Tram.codes[80], 0x2284);
    CHECK_EQ_INT(m_Tram.attributes[79], 0x43e1);
    CHECK_EQ_INT(m_Tram.attributes[80], 0x43e1);
    Tram_PutCharacter(32767, 0, &text.characters[0], 0x1234);
    CHECK_EQ_INT(m_Tram.codes[0], 0x2284);
    CHECK_EQ_INT(m_Tram.attributes[0], 0x1234);
    Arena_Free(&arena);
}

TEST(single_code_conversion_preserves_extended_character_word_arithmetic)
{
    ARENA arena;
    Arena_Init(&arena, "extended text code", 4096);
    const uint8_t bytes[] = { 0xfc, 0x40 };
    TRAM_TEXT text;
    CHECK(Result_Ignore(M_Read(bytes, sizeof(bytes), &arena, &text)));
    CHECK_EQ_INT(text.characters[0].code, 0x2177);
    CHECK_EQ_INT(text.characters[0].single_code, 0x21f7);

    Tram_Init();
    Tram_PutCharacter(0, 0, &text.characters[0], 0xe1);
    CHECK_EQ_INT(m_Tram.codes[0], 0x21f7);
    CHECK_EQ_INT(m_Tram.codes[1], 0x21f7);
    Arena_Free(&arena);
}
