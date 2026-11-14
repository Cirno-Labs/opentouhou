#include "stg/lzss.h"

#include "pbg3_builder.h"

#include "core/hash/crc32.h"
#include "test.h"

#include <string.h>

#define M_BIG_SIZE 0x6000

typedef enum {
    M_INPUT_RANDOM,
    M_INPUT_TEXT,
    M_INPUT_SPARSE,
} M_INPUT;

static uint8_t m_Input[M_BIG_SIZE];
static uint8_t m_Output[M_BIG_SIZE];

static uint8_t M_NextRandom(uint32_t *const state)
{
    *state = *state * 1103515245u + 12345u;
    return (uint8_t)(*state >> 16);
}

static size_t M_FillInput(const M_INPUT kind)
{
    uint32_t state = 12345;
    switch (kind) {
    case M_INPUT_RANDOM:
        for (size_t i = 0; i < 0x5000; i++) {
            m_Input[i] = M_NextRandom(&state);
        }
        return 0x5000;
    case M_INPUT_TEXT:
        for (size_t i = 0; i < 0x6000; i++) {
            m_Input[i] =
                (uint8_t)("the quick brown fox "[i % 20] + (i / 997) % 3);
        }
        return 0x6000;
    case M_INPUT_SPARSE:
        for (size_t i = 0; i < 0x4000; i++) {
            m_Input[i] = i % 7 == 0 ? M_NextRandom(&state) : 0;
        }
        return 0x4000;
    }
    return 0;
}

// Fills m_Input with size bytes below alphabet_size, drawn from seed.
static void M_FillAlphabet(
    const size_t size, const uint8_t alphabet_size, uint32_t seed)
{
    for (size_t i = 0; i < size; i++) {
        m_Input[i] = M_NextRandom(&seed) % alphabet_size;
    }
}

static uint8_t *M_Encode(
    const uint8_t *const data, const size_t size, size_t *const out_size)
{
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator, nullptr);
    Lzss_Encode(data, size, &writer);
    uint8_t *const bytes = Writer_TakeBuffer(&writer, out_size);
    Result_Ignore(Writer_Close(&writer));
    return bytes;
}

static bool M_EncodesAs(
    const uint8_t *const data, const size_t size, const uint8_t *const expected,
    const size_t expected_size)
{
    size_t encoded_size;
    uint8_t *const encoded = M_Encode(data, size, &encoded_size);
    const bool same = encoded_size == expected_size
        && (expected_size == 0
            || memcmp(encoded, expected, expected_size) == 0);
    Allocator_Free(&g_HeapAllocator, encoded, encoded_size);
    return same;
}

static bool M_EncodesWithCrc(
    const size_t size, const size_t expected_size, const uint32_t expected_crc)
{
    size_t encoded_size;
    uint8_t *const encoded = M_Encode(m_Input, size, &encoded_size);
    const bool same = encoded_size == expected_size
        && Crc32_Compute(encoded, encoded_size) == expected_crc;
    Allocator_Free(&g_HeapAllocator, encoded, encoded_size);
    return same;
}

static bool M_RoundTrips(const uint8_t *const data, const size_t size)
{
    size_t encoded_size;
    uint8_t *const encoded = M_Encode(data, size, &encoded_size);
    READER reader;
    Reader_OpenBuffer(&reader, encoded, encoded_size, nullptr);
    size_t decoded;
    const RESULT result =
        Lzss_Decode(&reader, m_Output, size, &decoded, nullptr);
    const bool same = IS_OK(result) && decoded == size
        && (size == 0 || memcmp(m_Output, data, size) == 0);
    Result_Ignore(result);
    Allocator_Free(&g_HeapAllocator, encoded, encoded_size);
    return same;
}

static RESULT M_Decode(
    const PBG3_BUILDER_WRITER *const stream, uint8_t *const output,
    const size_t size, size_t *const decoded, uint32_t *const checksum)
{
    READER reader;
    Reader_OpenBuffer(&reader, stream->bytes, stream->length, nullptr);
    return Lzss_Decode(&reader, output, size, decoded, checksum);
}

TEST(literals_decode)
{
    PBG3_BUILDER_WRITER bw;
    Pbg3Builder_Init(&bw);
    for (const char *c = "hello"; *c != '\0'; c++) {
        Pbg3Builder_WriteLiteral(&bw, (uint8_t)*c);
    }
    Pbg3Builder_FlushByte(&bw);

    uint8_t output[5];
    size_t decoded;
    CHECK(Result_Log(
        M_Decode(&bw, output, sizeof(output), &decoded, nullptr),
        LOG_LEVEL_ERROR));
    CHECK_EQ_INT(decoded, 5);
    CHECK(memcmp(output, "hello", 5) == 0);
}

TEST(a_match_copies_from_the_window_that_starts_at_position_1)
{
    PBG3_BUILDER_WRITER bw;
    Pbg3Builder_Init(&bw);
    Pbg3Builder_WriteLiteral(&bw, 'A');
    Pbg3Builder_WriteLiteral(&bw, 'B');
    Pbg3Builder_WriteLiteral(&bw, 'C');
    Pbg3Builder_WriteBackref(&bw, 2, 3);
    Pbg3Builder_WriteBackref(&bw, 1, 4);
    Pbg3Builder_FlushByte(&bw);

    uint8_t output[10];
    size_t decoded;
    CHECK(Result_Log(
        M_Decode(&bw, output, sizeof(output), &decoded, nullptr),
        LOG_LEVEL_ERROR));
    CHECK_EQ_INT(decoded, 10);
    CHECK(memcmp(output, "ABCBCBABCB", 10) == 0);
}

TEST(a_match_reads_bytes_it_writes)
{
    PBG3_BUILDER_WRITER bw;
    Pbg3Builder_Init(&bw);
    Pbg3Builder_WriteLiteral(&bw, 'x');
    Pbg3Builder_WriteBackref(&bw, 1, 18);
    Pbg3Builder_FlushByte(&bw);

    uint8_t output[19];
    size_t decoded;
    CHECK(Result_Log(
        M_Decode(&bw, output, sizeof(output), &decoded, nullptr),
        LOG_LEVEL_ERROR));
    CHECK_EQ_INT(decoded, 19);
    for (size_t i = 0; i < sizeof(output); i++) {
        CHECK_EQ_INT(output[i], 'x');
    }
}

TEST(the_window_wraps_at_0x2000)
{
    PBG3_BUILDER_WRITER bw;
    Pbg3Builder_Init(&bw);
    const size_t literal_count = 0x2005;
    for (size_t i = 0; i < literal_count; i++) {
        Pbg3Builder_WriteLiteral(&bw, (uint8_t)(i * 7 + (i >> 8)));
    }
    // Byte k lies at window position (k + 1) & 0x1fff, so the match reads
    // bytes 0x1ffd to 0x2001 across both wraps.
    Pbg3Builder_WriteBackref(&bw, 0x1ffe, 5);
    Pbg3Builder_FlushByte(&bw);

    static uint8_t output[0x2005 + 5];
    size_t decoded;
    CHECK(Result_Log(
        M_Decode(&bw, output, sizeof(output), &decoded, nullptr),
        LOG_LEVEL_ERROR));
    CHECK_EQ_INT(decoded, sizeof(output));
    for (size_t i = 0; i < 5; i++) {
        const size_t source = 0x1ffd + i;
        CHECK_EQ_INT(
            output[literal_count + i], (uint8_t)(source * 7 + (source >> 8)));
    }
}

TEST(offset_0_ends_the_stream)
{
    PBG3_BUILDER_WRITER bw;
    Pbg3Builder_Init(&bw);
    Pbg3Builder_WriteLiteral(&bw, 'a');
    Pbg3Builder_WriteStop(&bw);
    Pbg3Builder_WriteLiteral(&bw, 'b');
    Pbg3Builder_FlushByte(&bw);

    uint8_t output[4];
    size_t decoded;
    CHECK(Result_Log(
        M_Decode(&bw, output, sizeof(output), &decoded, nullptr),
        LOG_LEVEL_ERROR));
    CHECK_EQ_INT(decoded, 1);
    CHECK_EQ_INT(output[0], 'a');
}

TEST(decoding_stops_at_size)
{
    PBG3_BUILDER_WRITER bw;
    Pbg3Builder_Init(&bw);
    Pbg3Builder_WriteLiteral(&bw, 'a');
    Pbg3Builder_WriteLiteral(&bw, 'b');
    Pbg3Builder_FlushByte(&bw);

    uint8_t output[1];
    size_t decoded;
    CHECK(Result_Log(
        M_Decode(&bw, output, sizeof(output), &decoded, nullptr),
        LOG_LEVEL_ERROR));
    CHECK_EQ_INT(decoded, 1);
    CHECK_EQ_INT(output[0], 'a');
}

TEST(a_match_past_size_fails)
{
    PBG3_BUILDER_WRITER bw;
    Pbg3Builder_Init(&bw);
    Pbg3Builder_WriteLiteral(&bw, 'a');
    Pbg3Builder_WriteBackref(&bw, 1, 3);
    Pbg3Builder_FlushByte(&bw);

    uint8_t output[3];
    size_t decoded;
    const RESULT result =
        M_Decode(&bw, output, sizeof(output), &decoded, nullptr);
    CHECK(!IS_OK(result));
    Result_Ignore(result);
}

TEST(a_match_that_ends_at_size_decodes)
{
    PBG3_BUILDER_WRITER bw;
    Pbg3Builder_Init(&bw);
    Pbg3Builder_WriteLiteral(&bw, 'a');
    Pbg3Builder_WriteBackref(&bw, 1, 3);
    Pbg3Builder_FlushByte(&bw);

    uint8_t output[4];
    size_t decoded;
    CHECK(Result_Log(
        M_Decode(&bw, output, sizeof(output), &decoded, nullptr),
        LOG_LEVEL_ERROR));
    CHECK_EQ_INT(decoded, 4);
    CHECK(memcmp(output, "aaaa", 4) == 0);
}

TEST(the_checksum_sums_only_the_bytes_read)
{
    PBG3_BUILDER_WRITER bw;
    Pbg3Builder_Init(&bw);
    Pbg3Builder_WriteLiteral(&bw, 0xff);
    Pbg3Builder_WriteLiteral(&bw, 0xff);
    Pbg3Builder_FlushByte(&bw);
    CHECK_EQ_INT(bw.length, 3);

    // The first literal takes 9 bits, so 2 of the 3 bytes are read.
    uint8_t output[1];
    size_t decoded;
    uint32_t checksum;
    CHECK(Result_Log(
        M_Decode(&bw, output, sizeof(output), &decoded, &checksum),
        LOG_LEVEL_ERROR));
    CHECK_EQ_INT(checksum, bw.bytes[0] + bw.bytes[1]);
}

TEST(bits_past_the_end_of_input_read_as_0)
{
    // th07's encoding of no data: the end marker without its last 6 bits.
    const uint8_t stream[] = { 0x00 };
    READER reader;
    Reader_OpenBuffer(&reader, stream, sizeof(stream), nullptr);
    uint8_t output[4];
    size_t decoded = 99;
    CHECK(Result_Log(
        Lzss_Decode(&reader, output, sizeof(output), &decoded, nullptr),
        LOG_LEVEL_ERROR));
    CHECK_EQ_INT(decoded, 0);
}

TEST(encoding_matches_the_original_for_short_inputs)
{
    static const uint8_t empty[] = { 0x00 };
    CHECK(M_EncodesAs(nullptr, 0, empty, sizeof(empty)));

    static const uint8_t one[] = { 0xa0, 0x80 };
    CHECK(M_EncodesAs((const uint8_t *)"A", 1, one, sizeof(one)));

    static const uint8_t abc[] = { 0xa0, 0xd0, 0xa8, 0x60, 0x00, 0xb0, 0x00 };
    CHECK(M_EncodesAs((const uint8_t *)"ABCABCABCABC", 12, abc, sizeof(abc)));

    static const uint8_t hello[] = {
        0xb4, 0x59, 0x6d, 0x96, 0xcb, 0x7c, 0xb2, 0x40, 0x00,
        0x0c, 0xc8, 0x2e, 0xf6, 0xfb, 0x95, 0xb2, 0xc8, 0x00,
    };
    CHECK(M_EncodesAs(
        (const uint8_t *)"hello, hello, hello world", 25, hello,
        sizeof(hello)));

    uint8_t zs[100];
    memset(zs, 'z', sizeof(zs));
    static const uint8_t z100[] = {
        0xbd, 0x00, 0x03, 0xe0, 0x09, 0xf8, 0x04, 0xbe,
        0x01, 0xbf, 0x80, 0x93, 0xe0, 0x2d, 0xb0, 0x00,
    };
    CHECK(M_EncodesAs(zs, sizeof(zs), z100, sizeof(z100)));
}

TEST(encoding_matches_the_original_past_the_window_size)
{
    CHECK(M_EncodesWithCrc(M_FillInput(M_INPUT_RANDOM), 23028, 0x9aa17608));
    CHECK(M_EncodesWithCrc(M_FillInput(M_INPUT_TEXT), 3167, 0x640826ca));
    CHECK(M_EncodesWithCrc(M_FillInput(M_INPUT_SPARSE), 5181, 0x76753cff));
}

TEST(encoding_matches_the_original_when_node_0_leaves_the_tree)
{
    // Unlinking a leaf sets the parent of node 0, so the encoder deletes
    // node 0 when the window reaches position 0.
    M_FillAlphabet(9000, 3, 1152);
    CHECK(M_EncodesWithCrc(9000, 2787, 0x042d845c));
}

TEST(encoding_matches_the_original_when_a_replaced_node_leaves_the_window)
{
    // A full-length match replaces the older node in the tree and unlinks
    // it, so deleting it when the window moves past it does nothing.
    M_FillAlphabet(9000, 2, 106);
    CHECK(M_EncodesWithCrc(9000, 1718, 0xd5ed27c4));
}

TEST(encoding_matches_the_original_when_a_match_reaches_the_end)
{
    // A match found near the end is cut to the bytes left in the lookahead.
    M_FillAlphabet(20, 2, 1);
    static const uint8_t expected[] = {
        0x80, 0x40, 0x20, 0x20, 0x01, 0x84, 0x02, 0x02,
        0x00, 0x08, 0x80, 0x02, 0x70, 0x18, 0x00, 0x00,
    };
    CHECK(M_EncodesAs(m_Input, 20, expected, sizeof(expected)));
}

TEST(round_trips)
{
    CHECK(M_RoundTrips(nullptr, 0));
    CHECK(M_RoundTrips((const uint8_t *)"A", 1));

    memset(m_Input, 'z', 0x3000);
    CHECK(M_RoundTrips(m_Input, 0x3000));

    const M_INPUT kinds[] = { M_INPUT_RANDOM, M_INPUT_TEXT, M_INPUT_SPARSE };
    for (size_t i = 0; i < sizeof(kinds) / sizeof(kinds[0]); i++) {
        const size_t size = M_FillInput(kinds[i]);
        CHECK(M_RoundTrips(m_Input, size));
    }
}
