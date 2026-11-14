#include "core/io/reader.h"

#include "test.h"
#include "test_allocator.h"

static void *M_Copy(
    const char *const text, const size_t size, const ALLOCATOR *const allocator)
{
    void *const data = Allocator_Alloc(allocator, size);
    memcpy(data, text, size);
    return data;
}

TEST(reads_and_frees_with_no_leak)
{
    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    void *const data = M_Copy("hello", 5, &test.allocator);
    READER reader;
    Reader_OpenOwnedBuffer(&reader, data, 5, &test.allocator, nullptr);
    CHECK_EQ_INT(test.live_blocks, 1);
    uint8_t byte;
    Reader_Read(&reader, &byte);
    CHECK_EQ_INT(byte, 'h');
    CHECK(!Reader_Failed(&reader));
    Reader_Close(&reader);
    CHECK_EQ_INT(test.live_blocks, 0);
    CHECK_EQ_INT(test.size_errors, 0);
}

TEST(an_empty_owned_buffer_frees_with_no_leak)
{
    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    void *const data = Allocator_Alloc(&test.allocator, 0);
    READER reader;
    Reader_OpenOwnedBuffer(&reader, data, 0, &test.allocator, nullptr);
    CHECK_EQ_INT(Reader_Size(&reader), 0);
    Reader_Close(&reader);
    CHECK_EQ_INT(test.live_blocks, 0);
}

TEST(borrow_returns_a_pointer_into_the_owned_data)
{
    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    void *const data = M_Copy("hello", 5, &test.allocator);
    READER reader;
    Reader_OpenOwnedBuffer(&reader, data, 5, &test.allocator, nullptr);
    const void *const borrowed = Reader_Borrow(&reader, 5);
    CHECK(borrowed != nullptr && memcmp(borrowed, "hello", 5) == 0);
    Reader_Close(&reader);
}

TEST(a_slice_stays_valid_until_the_owner_closes)
{
    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    void *const data = M_Copy("hello world", 11, &test.allocator);
    READER reader;
    Reader_OpenOwnedBuffer(&reader, data, 11, &test.allocator, nullptr);
    READER slice = Reader_Slice(&reader, 5);
    const void *const borrowed = Reader_Borrow(&slice, 5);
    CHECK(borrowed != nullptr && memcmp(borrowed, "hello", 5) == 0);
    Reader_Close(&reader);
    CHECK_EQ_INT(test.live_blocks, 0);
}

TEST(an_owned_buffer_has_its_name_until_the_close)
{
    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    void *const data = M_Copy("hi", 2, &test.allocator);
    READER reader;
    Reader_OpenOwnedBuffer(&reader, data, 2, &test.allocator, "owned.dat");
    CHECK_EQ_STR(Reader_GetName(&reader), "owned.dat");
    Reader_ReadU32(&reader);
    Reader_Close(&reader);
    CHECK(Reader_GetName(&reader) == nullptr);
    CHECK_EQ_INT(test.live_blocks, 0);
    const RESULT result = Reader_Result(&reader);
    CHECK_EQ_STR(
        result.message, "read of 4 bytes at 0x0 past end 0x2 in owned.dat");
    Result_Ignore(result);
}
