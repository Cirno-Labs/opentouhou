#include "stg/pbg4.h"

#include "core/archive.h"
#include "core/base/cp932.h"
#include "core/io/writer.h"
#include "stg/lzss.h"
#include "test.h"
#include "test_allocator.h"

#include <string.h>

#define M_HEADER_SIZE 16

typedef struct {
    const char *name;
    const char *data;
    // Moves the recorded offset away from where the data lies.
    int32_t offset_shift;
} M_FILE;

typedef struct {
    uint8_t *bytes;
    size_t size;
} M_ARCHIVE;

// Builds a PBG4 archive of count files: the header, each file LZSS-packed,
// then the LZSS-packed table. Every checksum is 0xdeadbeef, which nothing
// checks. extra_size is added to the recorded size of the first file.
static M_ARCHIVE M_Build(
    const M_FILE *const files, const int32_t count, const uint32_t extra_size)
{
    WRITER table;
    Writer_OpenBuffer(&table, &g_HeapAllocator, nullptr);
    WRITER out;
    Writer_OpenBuffer(&out, &g_HeapAllocator, nullptr);
    static const uint8_t header[M_HEADER_SIZE] = { 0 };
    Writer_WriteBytes(&out, header, sizeof(header));
    for (int32_t i = 0; i < count; i++) {
        const size_t size = strlen(files[i].data);
        Writer_WriteBytes(&table, files[i].name, strlen(files[i].name) + 1);
        Writer_WriteU32(
            &table,
            (uint32_t)Writer_Tell(&out) + (uint32_t)files[i].offset_shift);
        Writer_WriteU32(&table, (uint32_t)size + (i == 0 ? extra_size : 0));
        Writer_WriteU32(&table, 0xdeadbeef);
        Lzss_Encode((const uint8_t *)files[i].data, size, &out);
    }
    size_t table_size;
    uint8_t *const table_bytes = Writer_TakeBuffer(&table, &table_size);
    Result_Ignore(Writer_Close(&table));

    const size_t table_offset = Writer_Tell(&out);
    Lzss_Encode(table_bytes, table_size, &out);
    Allocator_Free(&g_HeapAllocator, table_bytes, table_size);
    Writer_Seek(&out, 0);
    Writer_WriteBytes(&out, "PBG4", 4);
    Writer_WriteU32(&out, (uint32_t)count);
    Writer_WriteU32(&out, (uint32_t)table_offset);
    Writer_WriteU32(&out, (uint32_t)table_size);

    M_ARCHIVE archive;
    archive.bytes = Writer_TakeBuffer(&out, &archive.size);
    Result_Ignore(Writer_Close(&out));
    return archive;
}

static void M_Free(const M_ARCHIVE *const archive)
{
    Allocator_Free(&g_HeapAllocator, archive->bytes, archive->size);
}

static void M_SetU32(
    const M_ARCHIVE *const archive, const size_t offset, const uint32_t value)
{
    for (size_t i = 0; i < 4; i++) {
        archive->bytes[offset + i] = (uint8_t)(value >> (i * 8));
    }
}

static RESULT M_Open(
    const M_ARCHIVE *const bytes, const ALLOCATOR *const allocator,
    ARCHIVE **const out)
{
    READER reader;
    Reader_OpenBuffer(&reader, bytes->bytes, bytes->size, "test.dat");
    return Archive_Open(&g_Pbg4Format, &reader, allocator, out);
}

static bool M_ReadsAs(
    ARCHIVE *const archive, const ALLOCATOR *const allocator,
    const char *const path, const char *const expected)
{
    const ARCHIVE_ENTRY *const entry = Archive_Find(archive, path);
    if (entry == nullptr || ArchiveEntry_GetSize(entry) != strlen(expected)) {
        return false;
    }
    uint8_t *data;
    if (!Result_Log(Archive_Read(archive, entry, &data), LOG_LEVEL_ERROR)) {
        return false;
    }
    const bool same = memcmp(data, expected, strlen(expected)) == 0;
    Allocator_Free(allocator, data, strlen(expected));
    return same;
}

static void M_CheckOpenFails(
    const M_ARCHIVE *const bytes, const char *const message)
{
    ARCHIVE *archive;
    const RESULT result = M_Open(bytes, &g_HeapAllocator, &archive);
    CHECK_EQ_STR(result.message, message);
    Result_Ignore(result);
}

TEST(entries_read_back)
{
    const M_FILE files[] = {
        { "a.txt", "hello, hello, hello world", 0 },
        { "b.txt", "", 0 },
        { "c.txt", "the last entry ends at the table", 0 },
    };
    const M_ARCHIVE bytes = M_Build(files, 3, 0);

    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    ARCHIVE *archive;
    CHECK(
        Result_Log(M_Open(&bytes, &test.allocator, &archive), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(Archive_GetCount(archive), 3);
    CHECK_EQ_STR(ArchiveEntry_GetName(Archive_GetEntry(archive, 2)), "c.txt");
    CHECK(M_ReadsAs(archive, &test.allocator, "a.txt", files[0].data));
    CHECK(M_ReadsAs(archive, &test.allocator, "b.txt", files[1].data));
    CHECK(M_ReadsAs(archive, &test.allocator, "c.txt", files[2].data));
    Archive_Close(archive);
    CHECK_EQ_INT(test.live_blocks, 0);
    M_Free(&bytes);
}

TEST(an_entry_ends_where_the_next_one_starts)
{
    // Moves the start of b.txt back by one byte. That byte holds the last bit
    // of the literal 'a' (0x61), which then reads as 0.
    const M_FILE files[] = {
        { "a.txt", "a", 0 },
        { "b.txt", "b", -1 },
    };
    const M_ARCHIVE bytes = M_Build(files, 2, 0);

    ARCHIVE *archive;
    CHECK(Result_Log(
        M_Open(&bytes, &g_HeapAllocator, &archive), LOG_LEVEL_ERROR));
    CHECK(M_ReadsAs(archive, &g_HeapAllocator, "a.txt", "\x60"));
    Archive_Close(archive);
    M_Free(&bytes);
}

TEST(an_entry_that_ends_before_it_starts_fails)
{
    const M_FILE files[] = {
        { "a.txt", "a", 0 },
        { "b.txt", "b", -16 },
    };
    const M_ARCHIVE bytes = M_Build(files, 2, 0);
    M_CheckOpenFails(
        &bytes, "test.dat: a.txt: entry data ends before it starts");
    M_Free(&bytes);
}

TEST(lookup_is_by_last_component_and_ignores_ascii_case)
{
    const M_FILE files[] = { { "Title00.jpg", "v", 0 } };
    const M_ARCHIVE bytes = M_Build(files, 1, 0);

    ARCHIVE *archive;
    CHECK(Result_Log(
        M_Open(&bytes, &g_HeapAllocator, &archive), LOG_LEVEL_ERROR));
    CHECK(M_ReadsAs(archive, &g_HeapAllocator, "data/title00.jpg", "v"));
    CHECK(M_ReadsAs(archive, &g_HeapAllocator, "TITLE00.JPG", "v"));
    CHECK(Archive_Find(archive, "title01.jpg") == nullptr);
    Archive_Close(archive);
    M_Free(&bytes);
}

TEST(a_cp932_name_is_converted_to_utf8)
{
    char *cp932_name;
    CHECK(Result_Log(
        Cp932_Encode(
            "\xe3\x81\x82.txt", strlen("\xe3\x81\x82.txt"), &cp932_name),
        LOG_LEVEL_ERROR));
    const M_FILE files[] = { { cp932_name, "w", 0 } };
    const M_ARCHIVE bytes = M_Build(files, 1, 0);
    Mem_Free(cp932_name);

    ARCHIVE *archive;
    CHECK(Result_Log(
        M_Open(&bytes, &g_HeapAllocator, &archive), LOG_LEVEL_ERROR));
    CHECK_EQ_STR(
        ArchiveEntry_GetName(Archive_GetEntry(archive, 0)), "\xe3\x81\x82.txt");
    CHECK(M_ReadsAs(archive, &g_HeapAllocator, "\xe3\x81\x82.txt", "w"));
    Archive_Close(archive);
    M_Free(&bytes);
}

TEST(a_bad_magic_fails)
{
    const M_FILE files[] = { { "a.txt", "a", 0 } };
    const M_ARCHIVE bytes = M_Build(files, 1, 0);
    bytes.bytes[3] = '3';
    M_CheckOpenFails(&bytes, "test.dat: bad magic");
    M_Free(&bytes);
}

TEST(no_entries_fail)
{
    const M_FILE files[] = { { "a.txt", "a", 0 } };
    const M_ARCHIVE bytes = M_Build(files, 1, 0);
    M_SetU32(&bytes, 4, 0);
    M_CheckOpenFails(&bytes, "test.dat: archive has no entries");
    M_SetU32(&bytes, 4, 0x80000000);
    M_CheckOpenFails(&bytes, "test.dat: archive has no entries");
    M_Free(&bytes);
}

TEST(a_table_past_the_end_fails)
{
    const M_FILE files[] = { { "a.txt", "a", 0 } };
    const M_ARCHIVE bytes = M_Build(files, 1, 0);
    M_SetU32(&bytes, 8, (uint32_t)bytes.size);
    M_CheckOpenFails(
        &bytes, "test.dat: table runs past the end of the archive");
    M_Free(&bytes);
}

TEST(a_table_shorter_than_recorded_fails)
{
    const M_FILE files[] = { { "a.txt", "a", 0 } };
    const M_ARCHIVE bytes = M_Build(files, 1, 0);
    M_SetU32(&bytes, 12, 5 + 1 + 12 + 1);
    M_CheckOpenFails(&bytes, "test.dat: table is shorter than recorded");
    M_SetU32(&bytes, 12, 0x7fffffff);
    M_CheckOpenFails(
        &bytes, "test.dat: table is longer than its packed data can hold");
    M_Free(&bytes);
}

TEST(more_entries_than_the_table_holds_fail)
{
    const M_FILE files[] = { { "a.txt", "a", 0 } };
    const M_ARCHIVE bytes = M_Build(files, 1, 0);
    M_SetU32(&bytes, 4, 2);
    M_CheckOpenFails(&bytes, "test.dat: table runs past its unpacked size");
    M_Free(&bytes);
}

TEST(a_truncated_entry_fails)
{
    const M_FILE files[] = { { "a.txt", "abc", 0 } };
    const M_ARCHIVE bytes = M_Build(files, 1, 1);

    ARCHIVE *archive;
    CHECK(Result_Log(
        M_Open(&bytes, &g_HeapAllocator, &archive), LOG_LEVEL_ERROR));
    uint8_t *data;
    const RESULT result =
        Archive_Read(archive, Archive_GetEntry(archive, 0), &data);
    CHECK_EQ_STR(
        result.message,
        "test.dat: a.txt: decoded data is shorter than recorded");
    Result_Ignore(result);
    Archive_Close(archive);
    M_Free(&bytes);
}
