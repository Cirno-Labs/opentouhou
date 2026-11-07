#include "core/base/utils.h"
#include "core/io/reader.h"

#include "test.h"
#include "test_allocator.h"

typedef struct {
    const char *name;
    size_t size;
    uint64_t (*read)(READER *reader);
} READ_FUNC;

static uint64_t M_ReadU8(READER *const reader)
{
    return Reader_ReadU8(reader);
}

static uint64_t M_ReadU16(READER *const reader)
{
    return Reader_ReadU16(reader);
}

static uint64_t M_ReadU32(READER *const reader)
{
    return Reader_ReadU32(reader);
}

static uint64_t M_ReadU64(READER *const reader)
{
    return Reader_ReadU64(reader);
}

static uint64_t M_ReadI8(READER *const reader)
{
    return (uint64_t)Reader_ReadI8(reader);
}

static uint64_t M_ReadI16(READER *const reader)
{
    return (uint64_t)Reader_ReadI16(reader);
}

static uint64_t M_ReadI32(READER *const reader)
{
    return (uint64_t)Reader_ReadI32(reader);
}

static uint64_t M_ReadI64(READER *const reader)
{
    return (uint64_t)Reader_ReadI64(reader);
}

static uint64_t M_ReadF32(READER *const reader)
{
    const float value = Reader_ReadF32(reader);
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static uint64_t M_ReadF64(READER *const reader)
{
    const double value = Reader_ReadF64(reader);
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static const READ_FUNC m_ReadFuncs[] = {
    { "u8", 1, M_ReadU8 },   { "u16", 2, M_ReadU16 }, { "u32", 4, M_ReadU32 },
    { "u64", 8, M_ReadU64 }, { "i8", 1, M_ReadI8 },   { "i16", 2, M_ReadI16 },
    { "i32", 4, M_ReadI32 }, { "i64", 8, M_ReadI64 }, { "f32", 4, M_ReadF32 },
    { "f64", 8, M_ReadF64 },
};

static const uint8_t m_Counting[] = {
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
};

static bool M_FailsWith(const READER *const reader, const char *const message)
{
    const RESULT result = Reader_Result(reader);
    const bool matches = !IS_OK(result) && strcmp(result.message, message) == 0;
    if (!IS_OK(result) && !matches) {
        printf("    message: %s\n", result.message);
    }
    Result_Ignore(result);
    return matches;
}

TEST(unsigned_values_are_little_endian)
{
    READER reader;
    Reader_OpenBuffer(&reader, m_Counting, sizeof(m_Counting));
    CHECK_EQ_INT(Reader_ReadU8(&reader), 0x01);
    CHECK_EQ_INT(Reader_ReadU16(&reader), 0x0302);
    CHECK_EQ_INT(Reader_ReadU32(&reader), 0x07060504);
    CHECK(Reader_ReadU64(&reader) == 0x0F0E0D0C0B0A0908);
    CHECK_EQ_INT(Reader_Tell(&reader), 15);
    CHECK_EQ_INT(Reader_Size(&reader), 16);
    CHECK_EQ_INT(Reader_Remaining(&reader), 1);
    CHECK(Result_Ignore(Reader_Result(&reader)));
}

TEST(signed_values_are_twos_complement)
{
    const uint8_t data[] = {
        0xFF, 0x00, 0x80, 0xFE, 0xFF, 0xFF, 0xFF, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x7F,
    };
    READER reader;
    Reader_OpenBuffer(&reader, data, sizeof(data));
    CHECK_EQ_INT(Reader_ReadI8(&reader), -1);
    CHECK_EQ_INT(Reader_ReadI16(&reader), INT16_MIN);
    CHECK_EQ_INT(Reader_ReadI32(&reader), -2);
    CHECK(Reader_ReadI64(&reader) == INT64_MIN);
    CHECK_EQ_INT(Reader_ReadI8(&reader), 127);
    CHECK(!Reader_Failed(&reader));
}

TEST(read_picks_the_function_by_type)
{
    const uint8_t data[] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
        0x0C, 0x0D, 0x0E, 0x0F, 0xFF, 0x00, 0x80, 0xFE, 0xFF, 0xFF, 0xFF,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x80,
        0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC0,
    };
    READER reader;
    Reader_OpenBuffer(&reader, data, sizeof(data));
    uint8_t u8;
    uint16_t u16;
    uint32_t u32;
    uint64_t u64;
    int8_t i8;
    int16_t i16;
    int32_t i32;
    int64_t i64;
    float f32;
    double f64;
    Reader_Read(&reader, &u8);
    Reader_Read(&reader, &u16);
    Reader_Read(&reader, &u32);
    Reader_Read(&reader, &u64);
    Reader_Read(&reader, &i8);
    Reader_Read(&reader, &i16);
    Reader_Read(&reader, &i32);
    Reader_Read(&reader, &i64);
    Reader_Read(&reader, &f32);
    Reader_Read(&reader, &f64);
    CHECK_EQ_INT(u8, 0x01);
    CHECK_EQ_INT(u16, 0x0302);
    CHECK_EQ_INT(u32, 0x07060504);
    CHECK(u64 == 0x0F0E0D0C0B0A0908);
    CHECK_EQ_INT(i8, -1);
    CHECK_EQ_INT(i16, INT16_MIN);
    CHECK_EQ_INT(i32, -2);
    CHECK(i64 == INT64_MIN);
    CHECK(f32 == 1.0f);
    CHECK(f64 == -2.0);
    CHECK_EQ_INT(Reader_Remaining(&reader), 0);
    CHECK(!Reader_Failed(&reader));
}

TEST(floats_keep_their_bits)
{
    const uint32_t singles[] = {
        0x7FC00001, // quiet NaN with a payload
        0x7F800001, // signalling NaN
        0xFFBADBAD, // negative signalling NaN with a payload
        0x80000000, // negative zero
        0x00000001, // smallest subnormal
        0x807FFFFF, // largest negative subnormal
        0xFF800000, // negative infinity
        0x3F800000, // 1.0
    };
    const uint64_t doubles[] = {
        0x7FF8DEADBEEF0001, 0x7FF0000000000001, 0x8000000000000000,
        0x0000000000000001, 0x800FFFFFFFFFFFFF, 0x3FF0000000000000,
    };
    for (size_t i = 0; i < ARRAY_SIZE(singles); i++) {
        uint8_t data[4];
        for (size_t b = 0; b < sizeof(data); b++) {
            data[b] = (uint8_t)(singles[i] >> (8 * b));
        }
        READER reader;
        Reader_OpenBuffer(&reader, data, sizeof(data));
        float value;
        Reader_Read(&reader, &value);
        uint32_t bits;
        memcpy(&bits, &value, sizeof(bits));
        CHECK(bits == singles[i]);
    }
    for (size_t i = 0; i < ARRAY_SIZE(doubles); i++) {
        uint8_t data[8];
        for (size_t b = 0; b < sizeof(data); b++) {
            data[b] = (uint8_t)(doubles[i] >> (8 * b));
        }
        READER reader;
        Reader_OpenBuffer(&reader, data, sizeof(data));
        CHECK(M_ReadF64(&reader) == doubles[i]);
    }
}

TEST(every_width_fails_one_byte_short_and_names_the_offset)
{
    for (size_t i = 0; i < ARRAY_SIZE(m_ReadFuncs); i++) {
        const READ_FUNC *const func = &m_ReadFuncs[i];
        READER reader;
        Reader_OpenBuffer(&reader, m_Counting, 3 + func->size - 1);
        Reader_Skip(&reader, 3);
        const uint64_t value = func->read(&reader);
        char expected[64];
        snprintf(
            expected, sizeof(expected),
            "read of %zu bytes at 0x3 past end 0x%zx", func->size,
            3 + func->size - 1);
        if (value != 0 || !Reader_Failed(&reader) || Reader_Tell(&reader) != 3
            || !M_FailsWith(&reader, expected)) {
            TEST_FAIL("%s", func->name);
        }
    }
}

TEST(every_width_fails_on_empty_data)
{
    for (size_t i = 0; i < ARRAY_SIZE(m_ReadFuncs); i++) {
        READER reader;
        Reader_OpenBuffer(&reader, nullptr, 0);
        if (m_ReadFuncs[i].read(&reader) != 0 || !Reader_Failed(&reader)) {
            TEST_FAIL("%s", m_ReadFuncs[i].name);
        }
    }
}

TEST(every_width_reads_up_to_the_end)
{
    for (size_t i = 0; i < ARRAY_SIZE(m_ReadFuncs); i++) {
        READER reader;
        Reader_OpenBuffer(&reader, m_Counting, m_ReadFuncs[i].size);
        m_ReadFuncs[i].read(&reader);
        if (Reader_Failed(&reader) || Reader_Remaining(&reader) != 0) {
            TEST_FAIL("%s", m_ReadFuncs[i].name);
        }
    }
}

TEST(reads_after_a_failure_return_zero)
{
    READER reader;
    Reader_OpenBuffer(&reader, m_Counting, 3);
    uint8_t value = 0xAA;
    CHECK_EQ_INT(Reader_ReadU32(&reader), 0);
    CHECK_EQ_INT(Reader_ReadU8(&reader), 0);
    Reader_Read(&reader, &value);
    CHECK_EQ_INT(value, 0);
    CHECK(Reader_ReadF32(&reader) == 0.0f);
    CHECK_EQ_INT(Reader_Tell(&reader), 0);
    Reader_Seek(&reader, 2);
    Reader_Skip(&reader, 1);
    CHECK_EQ_INT(Reader_Tell(&reader), 0);
    CHECK(Reader_Borrow(&reader, 1) == nullptr);
}

TEST(only_the_first_failure_is_reported)
{
    READER reader;
    Reader_OpenBuffer(&reader, m_Counting, 6);
    Reader_Skip(&reader, 4);
    Reader_ReadU32(&reader);
    Reader_Seek(&reader, 100);
    Reader_ReadU64(&reader);
    CHECK(M_FailsWith(&reader, "read of 4 bytes at 0x4 past end 0x6"));
}

TEST(seek_tell_and_skip_move_the_position)
{
    READER reader;
    Reader_OpenBuffer(&reader, m_Counting, sizeof(m_Counting));
    Reader_Seek(&reader, 10);
    CHECK_EQ_INT(Reader_ReadU8(&reader), 0x0B);
    Reader_Skip(&reader, 2);
    CHECK_EQ_INT(Reader_Tell(&reader), 13);
    Reader_Seek(&reader, 0);
    CHECK_EQ_INT(Reader_ReadU8(&reader), 0x01);
    Reader_Seek(&reader, sizeof(m_Counting));
    CHECK_EQ_INT(Reader_Remaining(&reader), 0);
    CHECK(!Reader_Failed(&reader));
}

TEST(seeking_past_the_end_fails)
{
    READER reader;
    Reader_OpenBuffer(&reader, m_Counting, 8);
    Reader_Seek(&reader, 9);
    CHECK(M_FailsWith(&reader, "seek to 0x9 past end 0x8"));
    CHECK_EQ_INT(Reader_Tell(&reader), 0);
}

TEST(skipping_past_the_end_fails)
{
    READER reader;
    Reader_OpenBuffer(&reader, m_Counting, 8);
    Reader_Skip(&reader, 6);
    Reader_Skip(&reader, 3);
    CHECK(M_FailsWith(&reader, "skip of 3 bytes at 0x6 past end 0x8"));
    CHECK_EQ_INT(Reader_Tell(&reader), 6);
}

TEST(huge_sizes_do_not_wrap_around)
{
    READER reader;
    Reader_OpenBuffer(&reader, m_Counting, 8);
    Reader_Skip(&reader, 4);
    Reader_Skip(&reader, SIZE_MAX);
    CHECK(Reader_Failed(&reader));
    CHECK_EQ_INT(Reader_Tell(&reader), 4);

    Reader_OpenBuffer(&reader, m_Counting, 8);
    Reader_Skip(&reader, 4);
    const READER slice = Reader_Slice(&reader, SIZE_MAX - 2);
    CHECK(Reader_Failed(&reader));
    CHECK(Reader_Failed(&slice));
}

TEST(borrow_points_into_the_buffer)
{
    READER reader;
    Reader_OpenBuffer(&reader, m_Counting, sizeof(m_Counting));
    Reader_Skip(&reader, 2);
    CHECK(Reader_Borrow(&reader, 4) == m_Counting + 2);
    CHECK(Reader_Borrow(&reader, 0) == m_Counting + 6);
    CHECK_EQ_INT(Reader_Tell(&reader), 6);
    CHECK(Reader_Borrow(&reader, 11) == nullptr);
    CHECK(M_FailsWith(&reader, "read of 11 bytes at 0x6 past end 0x10"));
    CHECK_EQ_INT(Reader_Tell(&reader), 6);
}

TEST(read_bytes_copies_or_zeroes)
{
    READER reader;
    Reader_OpenBuffer(&reader, m_Counting, 6);
    uint8_t buffer[4];
    Reader_ReadBytes(&reader, buffer, sizeof(buffer));
    CHECK(memcmp(buffer, m_Counting, sizeof(buffer)) == 0);
    Reader_ReadBytes(&reader, buffer, sizeof(buffer));
    CHECK(memcmp(buffer, "\0\0\0\0", sizeof(buffer)) == 0);
    CHECK(Reader_Failed(&reader));
}

TEST(read_alloc_copies_into_an_owned_block)
{
    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    READER reader;
    Reader_OpenBuffer(&reader, m_Counting, sizeof(m_Counting));
    Reader_Skip(&reader, 4);
    uint8_t *const block = Reader_ReadAlloc(&reader, 8, &test.allocator);
    CHECK(block != nullptr && block != m_Counting + 4);
    CHECK(block != nullptr && memcmp(block, m_Counting + 4, 8) == 0);
    CHECK_EQ_INT(Reader_Tell(&reader), 12);
    CHECK_EQ_INT(test.live_blocks, 1);
    Allocator_Free(&test.allocator, block, 8);

    CHECK(Reader_ReadAlloc(&reader, SIZE_MAX, &test.allocator) == nullptr);
    CHECK(M_FailsWith(
        &reader, "read of 18446744073709551615 bytes at 0xc past end 0x10"));
    CHECK(Reader_ReadAlloc(&reader, 1, &test.allocator) == nullptr);
    CHECK_EQ_INT(test.live_blocks, 0);
    CHECK_EQ_INT(test.size_errors, 0);
}

TEST(slice_takes_the_next_bytes)
{
    READER reader;
    Reader_OpenBuffer(&reader, m_Counting, sizeof(m_Counting));
    Reader_Skip(&reader, 4);
    READER slice = Reader_Slice(&reader, 4);
    CHECK_EQ_INT(Reader_Tell(&reader), 8);
    CHECK_EQ_INT(Reader_Tell(&slice), 0);
    CHECK_EQ_INT(Reader_Size(&slice), 4);
    CHECK_EQ_INT(Reader_ReadU32(&slice), 0x08070605);
    CHECK_EQ_INT(Reader_ReadU8(&reader), 0x09);
    Reader_Seek(&slice, 1);
    CHECK(Reader_Borrow(&slice, 2) == m_Counting + 5);
    READER empty = Reader_Slice(&reader, 0);
    CHECK_EQ_INT(Reader_Size(&empty), 0);
    CHECK(!Reader_Failed(&reader) && !Reader_Failed(&slice));
}

TEST(a_failing_slice_names_the_outer_offset)
{
    READER reader;
    Reader_OpenBuffer(&reader, m_Counting, sizeof(m_Counting));
    Reader_Skip(&reader, 4);
    READER chunk = Reader_Slice(&reader, 8);
    Reader_Skip(&chunk, 2);
    READER entry = Reader_Slice(&chunk, 4);
    Reader_Seek(&entry, 2);
    Reader_ReadU32(&entry);
    CHECK(M_FailsWith(&entry, "read of 4 bytes at 0x8 past end 0xa"));
    CHECK(!Reader_Failed(&chunk));
    CHECK(!Reader_Failed(&reader));

    Reader_Seek(&chunk, 9);
    CHECK(M_FailsWith(&chunk, "seek to 0xd past end 0xc"));
}

TEST(a_slice_past_the_end_fails_both_readers)
{
    READER reader;
    Reader_OpenBuffer(&reader, m_Counting, sizeof(m_Counting));
    Reader_Skip(&reader, 8);
    READER chunk = Reader_Slice(&reader, 8);
    Reader_Skip(&chunk, 6);
    READER slice = Reader_Slice(&chunk, 4);
    const char *const expected = "slice of 4 bytes at 0xe past end 0x10";
    CHECK(M_FailsWith(&chunk, expected));
    CHECK(M_FailsWith(&slice, expected));
    CHECK_EQ_INT(Reader_Size(&slice), 0);
    CHECK_EQ_INT(Reader_ReadU8(&slice), 0);
    CHECK_EQ_INT(Reader_Tell(&chunk), 6);

    READER after = Reader_Slice(&chunk, 1);
    CHECK(M_FailsWith(&after, expected));
}
