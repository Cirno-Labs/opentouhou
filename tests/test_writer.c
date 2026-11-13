// SPDX-License-Identifier: GPL-3.0-only

#include "core/base/utils.h"
#include "core/io/reader.h"
#include "core/io/writer.h"

#include "test.h"
#include "test_allocator.h"

// Checks the bytes of a writer whose allocator is the heap's.
static bool M_Holds(
    WRITER *const writer, const void *const bytes, const size_t size)
{
    const size_t position = Writer_Tell(writer);
    size_t actual_size;
    uint8_t *const actual = Writer_TakeBuffer(writer, &actual_size);
    const bool matches =
        actual_size == size && memcmp(actual, bytes, size) == 0;
    Writer_WriteBytes(writer, actual, actual_size);
    Writer_Seek(writer, position);
    Allocator_Free(&g_HeapAllocator, actual, actual_size);
    return matches;
}

TEST(a_new_buffer_writer_is_empty_and_allocates_nothing)
{
    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    WRITER writer;
    Writer_OpenBuffer(&writer, &test.allocator);
    CHECK_EQ_INT(Writer_Tell(&writer), 0);
    CHECK_EQ_INT(test.live_blocks, 0);
    CHECK(Result_Ignore(Writer_Close(&writer)));
}

TEST(values_are_written_little_endian)
{
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator);
    Writer_WriteU8(&writer, 0x01);
    Writer_WriteU16(&writer, 0x0302);
    Writer_WriteU32(&writer, 0x07060504);
    Writer_WriteU64(&writer, 0x0F0E0D0C0B0A0908);
    Writer_WriteI8(&writer, -1);
    Writer_WriteI16(&writer, INT16_MIN);
    Writer_WriteI32(&writer, -2);
    Writer_WriteI64(&writer, INT64_MIN);
    Writer_WriteF32(&writer, 1.0f);
    Writer_WriteF64(&writer, -2.0);
    Writer_WriteBytes(&writer, "ab", 2);
    const uint8_t expected[] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
        0x0C, 0x0D, 0x0E, 0x0F, 0xFF, 0x00, 0x80, 0xFE, 0xFF, 0xFF, 0xFF,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x80,
        0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC0, 'a',  'b',
    };
    CHECK(M_Holds(&writer, expected, sizeof(expected)));
    CHECK(Result_Ignore(Writer_Close(&writer)));
}

TEST(write_picks_the_function_by_type_and_reads_back)
{
    const uint8_t u8 = UINT8_MAX;
    const uint16_t u16 = UINT16_MAX;
    const uint32_t u32 = UINT32_MAX;
    const uint64_t u64 = UINT64_MAX;
    int8_t i8 = INT8_MIN;
    int16_t i16 = INT16_MIN;
    int32_t i32 = INT32_MIN;
    int64_t i64 = INT64_MIN;
    const float f32 = 0.1f;
    double f64 = -0.1;
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator);
    Writer_Write(&writer, &u8);
    Writer_Write(&writer, &u16);
    Writer_Write(&writer, &u32);
    Writer_Write(&writer, &u64);
    Writer_Write(&writer, &i8);
    Writer_Write(&writer, &i16);
    Writer_Write(&writer, &i32);
    Writer_Write(&writer, &i64);
    Writer_Write(&writer, &f32);
    Writer_Write(&writer, &f64);
    CHECK_EQ_INT(Writer_Tell(&writer), 42);

    size_t size;
    uint8_t *const data = Writer_TakeBuffer(&writer, &size);
    READER reader;
    Reader_OpenBuffer(&reader, data, size);
    uint8_t r8;
    uint16_t r16;
    uint32_t r32;
    uint64_t r64;
    int8_t s8;
    int16_t s16;
    int32_t s32;
    int64_t s64;
    float rf32;
    double rf64;
    Reader_Read(&reader, &r8);
    Reader_Read(&reader, &r16);
    Reader_Read(&reader, &r32);
    Reader_Read(&reader, &r64);
    Reader_Read(&reader, &s8);
    Reader_Read(&reader, &s16);
    Reader_Read(&reader, &s32);
    Reader_Read(&reader, &s64);
    Reader_Read(&reader, &rf32);
    Reader_Read(&reader, &rf64);
    CHECK(r8 == u8 && r16 == u16 && r32 == u32 && r64 == u64);
    CHECK(s8 == i8 && s16 == i16 && s32 == i32 && s64 == i64);
    CHECK(rf32 == f32 && rf64 == f64);
    CHECK_EQ_INT(Reader_Remaining(&reader), 0);
    CHECK(Result_Ignore(Reader_Result(&reader)));
    Allocator_Free(&g_HeapAllocator, data, size);
    CHECK(Result_Ignore(Writer_Close(&writer)));
}

TEST(float_bits_survive_a_round_trip)
{
    const uint32_t singles[] = {
        0x7FC00001, 0x7F800001, 0xFFBADBAD, 0x80000000,
        0x00000001, 0x807FFFFF, 0x7F800000,
    };
    const uint64_t doubles[] = {
        0x7FF8DEADBEEF0001, 0x7FF0000000000001, 0xFFF0000000000001,
        0x8000000000000000, 0x0000000000000001, 0x800FFFFFFFFFFFFF,
    };
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator);
    for (size_t i = 0; i < ARRAY_SIZE(singles); i++) {
        float value;
        memcpy(&value, &singles[i], sizeof(value));
        Writer_Write(&writer, &value);
    }
    for (size_t i = 0; i < ARRAY_SIZE(doubles); i++) {
        double value;
        memcpy(&value, &doubles[i], sizeof(value));
        Writer_WriteF64(&writer, value);
    }

    size_t size;
    uint8_t *const data = Writer_TakeBuffer(&writer, &size);
    READER reader;
    Reader_OpenBuffer(&reader, data, size);
    for (size_t i = 0; i < ARRAY_SIZE(singles); i++) {
        const float value = Reader_ReadF32(&reader);
        uint32_t bits;
        memcpy(&bits, &value, sizeof(bits));
        CHECK(bits == singles[i]);
    }
    for (size_t i = 0; i < ARRAY_SIZE(doubles); i++) {
        double value;
        Reader_Read(&reader, &value);
        uint64_t bits;
        memcpy(&bits, &value, sizeof(bits));
        CHECK(bits == doubles[i]);
    }
    CHECK_EQ_INT(Reader_Remaining(&reader), 0);
    Allocator_Free(&g_HeapAllocator, data, size);
    CHECK(Result_Ignore(Writer_Close(&writer)));
}

TEST(seeking_back_patches_a_field)
{
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator);
    Writer_WriteBytes(&writer, "CHNK", 4);
    const size_t size_position = Writer_Tell(&writer);
    Writer_WriteU32(&writer, 0);
    for (int32_t i = 0; i < 5; i++) {
        Writer_WriteI32(&writer, i * 100);
    }
    const size_t end = Writer_Tell(&writer);
    Writer_Seek(&writer, size_position);
    Writer_WriteU32(&writer, (uint32_t)(end - size_position - 4));
    CHECK_EQ_INT(Writer_Tell(&writer), size_position + 4);
    Writer_Seek(&writer, end);
    Writer_WriteU8(&writer, 0xEE);

    size_t size;
    uint8_t *const data = Writer_TakeBuffer(&writer, &size);
    CHECK_EQ_INT(size, end + 1);
    READER reader;
    Reader_OpenBuffer(&reader, data, size);
    Reader_Skip(&reader, 4);
    READER chunk = Reader_Slice(&reader, Reader_ReadU32(&reader));
    CHECK_EQ_INT(Reader_Size(&chunk), 20);
    CHECK_EQ_INT(Reader_ReadI32(&chunk), 0);
    Reader_Skip(&chunk, 12);
    CHECK_EQ_INT(Reader_ReadI32(&chunk), 400);
    CHECK_EQ_INT(Reader_ReadU8(&reader), 0xEE);
    CHECK(!Reader_Failed(&reader) && !Reader_Failed(&chunk));
    Allocator_Free(&g_HeapAllocator, data, size);
    CHECK(Result_Ignore(Writer_Close(&writer)));
}

TEST(a_write_across_the_end_extends_the_data)
{
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator);
    Writer_WriteU32(&writer, 0x04030201);
    Writer_Seek(&writer, 2);
    Writer_WriteU32(&writer, 0x08070605);
    CHECK(M_Holds(&writer, "\x01\x02\x05\x06\x07\x08", 6));
    Writer_Seek(&writer, 0);
    Writer_WriteU8(&writer, 0xAA);
    CHECK(M_Holds(&writer, "\xAA\x02\x05\x06\x07\x08", 6));
    CHECK(Result_Ignore(Writer_Close(&writer)));
}

TEST(growth_keeps_every_byte)
{
    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    WRITER writer;
    Writer_OpenBuffer(&writer, &test.allocator);
    for (uint32_t i = 0; i < 10000; i++) {
        Writer_WriteU32(&writer, i);
    }
    CHECK_EQ_INT(Writer_Tell(&writer), 40000);
    CHECK_EQ_INT(test.live_blocks, 1);

    size_t size;
    uint8_t *const data = Writer_TakeBuffer(&writer, &size);
    CHECK_EQ_INT(size, 40000);
    READER reader;
    Reader_OpenBuffer(&reader, data, size);
    bool in_order = true;
    for (uint32_t i = 0; i < 10000; i++) {
        in_order = in_order && Reader_ReadU32(&reader) == i;
    }
    CHECK(in_order);
    Allocator_Free(&test.allocator, data, size);
    CHECK(Result_Ignore(Writer_Close(&writer)));
    CHECK_EQ_INT(test.live_blocks, 0);
    CHECK_EQ_INT(test.live_bytes, 0);
    CHECK_EQ_INT(test.size_errors, 0);
}

TEST(take_buffer_hands_over_exactly_the_bytes)
{
    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    WRITER writer;
    Writer_OpenBuffer(&writer, &test.allocator);
    Writer_WriteBytes(&writer, "abc", 3);
    size_t size;
    uint8_t *const data = Writer_TakeBuffer(&writer, &size);
    CHECK_EQ_INT(size, 3);
    CHECK(memcmp(data, "abc", 3) == 0);
    CHECK_EQ_INT(test.live_bytes, 3);
    CHECK_EQ_INT(Writer_Tell(&writer), 0);

    Writer_WriteU8(&writer, 7);
    size_t second_size;
    uint8_t *const second = Writer_TakeBuffer(&writer, &second_size);
    CHECK_EQ_INT(second_size, 1);
    CHECK_EQ_INT(second[0], 7);
    CHECK(Result_Ignore(Writer_Close(&writer)));
    CHECK_EQ_INT(test.live_blocks, 2);

    Allocator_Free(&test.allocator, data, size);
    Allocator_Free(&test.allocator, second, second_size);
    CHECK_EQ_INT(test.live_blocks, 0);
    CHECK_EQ_INT(test.size_errors, 0);
}

TEST(take_buffer_of_an_empty_writer_gives_an_empty_block)
{
    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    WRITER writer;
    Writer_OpenBuffer(&writer, &test.allocator);
    size_t size = 1;
    uint8_t *const data = Writer_TakeBuffer(&writer, &size);
    CHECK(data != nullptr);
    CHECK_EQ_INT(size, 0);
    Allocator_Free(&test.allocator, data, size);
    CHECK(Result_Ignore(Writer_Close(&writer)));
    CHECK_EQ_INT(test.live_blocks, 0);
    CHECK_EQ_INT(test.size_errors, 0);
}

TEST(close_releases_a_buffer_that_was_not_taken)
{
    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    WRITER writer;
    Writer_OpenBuffer(&writer, &test.allocator);
    Writer_WriteU64(&writer, 1);
    CHECK_EQ_INT(test.live_blocks, 1);
    CHECK(Result_Ignore(Writer_Close(&writer)));
    CHECK_EQ_INT(test.live_blocks, 0);
    CHECK_EQ_INT(test.size_errors, 0);
    CHECK(Result_Ignore(Writer_Close(&writer)));
}
