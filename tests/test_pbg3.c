#include "stg/pbg3.h"

#include "pbg3_builder.h"

#include "core/base/cp932.h"
#include "core/base/string.h"
#include "test.h"
#include "test_allocator.h"

#include <string.h>

static RESULT M_Open(
    const uint8_t *const bytes, const size_t size,
    const ALLOCATOR *const allocator, VFS_SOURCE **const out)
{
    READER *const reader = Mem_Alloc(sizeof(*reader));
    Reader_OpenBuffer(reader, bytes, size, nullptr);
    const RESULT result = Pbg3_Open(reader, "test.dat", allocator, out);
    Mem_Free(reader);
    return result;
}

static bool M_ReadsAs(READER *const reader, const char *const expected)
{
    const size_t size = Reader_Size(reader);
    char *const buffer = Mem_Alloc(size + 1);
    Reader_ReadBytes(reader, buffer, size);
    buffer[size] = '\0';
    const bool matches = strcmp(buffer, expected) == 0;
    Mem_Free(buffer);
    return matches && Result_Ignore(Reader_Result(reader));
}

TEST(a_literal_only_entry_decodes)
{
    uint8_t compressed[64];
    PBG3_BUILDER_WRITER bw;
    Pbg3Builder_Init(&bw);
    const char *const text = "hello";
    for (const char *c = text; *c != '\0'; c++) {
        Pbg3Builder_WriteLiteral(&bw, (uint8_t)*c);
    }
    Pbg3Builder_FlushByte(&bw);
    memcpy(compressed, bw.bytes, bw.length);

    const PBG3_BUILDER_ENTRY entry = {
        .name = "hello.txt",
        .compressed = compressed,
        .compressed_size = bw.length,
        .checksum = Pbg3Builder_Checksum(compressed, bw.length),
        .decoded_size = 5,
    };
    uint8_t archive[PBG3_BUILDER_MAX_ARCHIVE_SIZE];
    const size_t size = Pbg3Builder_BuildArchive(&entry, 1, archive);

    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    VFS_SOURCE *source;
    CHECK(Result_Log(
        M_Open(archive, size, &test.allocator, &source), LOG_LEVEL_ERROR));

    READER reader;
    CHECK(Result_Log(
        source->funcs->open(source, "hello.txt", VFS_IN_MEMORY, &reader),
        LOG_LEVEL_ERROR));
    CHECK(M_ReadsAs(&reader, "hello"));
    Reader_Close(&reader);
    source->funcs->close(source);
    CHECK_EQ_INT(test.live_blocks, 0);
}

TEST(an_overlapping_back_reference_repeats_a_byte)
{
    PBG3_BUILDER_WRITER bw;
    Pbg3Builder_Init(&bw);
    Pbg3Builder_WriteLiteral(&bw, 'A');
    Pbg3Builder_WriteBackref(
        &bw, 1, 8); // repeat dict[1] eight times, self-overlapping
    Pbg3Builder_FlushByte(&bw);

    const PBG3_BUILDER_ENTRY entry = {
        .name = "a.txt",
        .compressed = bw.bytes,
        .compressed_size = bw.length,
        .checksum = Pbg3Builder_Checksum(bw.bytes, bw.length),
        .decoded_size = 9,
    };
    uint8_t archive[PBG3_BUILDER_MAX_ARCHIVE_SIZE];
    const size_t size = Pbg3Builder_BuildArchive(&entry, 1, archive);

    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    VFS_SOURCE *source;
    CHECK(Result_Log(
        M_Open(archive, size, &test.allocator, &source), LOG_LEVEL_ERROR));

    READER reader;
    CHECK(Result_Log(
        source->funcs->open(source, "a.txt", VFS_IN_MEMORY, &reader),
        LOG_LEVEL_ERROR));
    CHECK(M_ReadsAs(&reader, "AAAAAAAAA"));
    Reader_Close(&reader);
    source->funcs->close(source);
    CHECK_EQ_INT(test.live_blocks, 0);
}

TEST(a_non_overlapping_back_reference_repeats_three_bytes)
{
    PBG3_BUILDER_WRITER bw;
    Pbg3Builder_Init(&bw);
    Pbg3Builder_WriteLiteral(&bw, 'A');
    Pbg3Builder_WriteLiteral(&bw, 'B');
    Pbg3Builder_WriteLiteral(&bw, 'C');
    Pbg3Builder_WriteBackref(&bw, 1, 3); // repeat dict[1..3] once: "ABC"
    Pbg3Builder_FlushByte(&bw);

    const PBG3_BUILDER_ENTRY entry = {
        .name = "b.txt",
        .compressed = bw.bytes,
        .compressed_size = bw.length,
        .checksum = Pbg3Builder_Checksum(bw.bytes, bw.length),
        .decoded_size = 6,
    };
    uint8_t archive[PBG3_BUILDER_MAX_ARCHIVE_SIZE];
    const size_t size = Pbg3Builder_BuildArchive(&entry, 1, archive);

    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    VFS_SOURCE *source;
    CHECK(Result_Log(
        M_Open(archive, size, &test.allocator, &source), LOG_LEVEL_ERROR));

    READER reader;
    CHECK(Result_Log(
        source->funcs->open(source, "b.txt", VFS_IN_MEMORY, &reader),
        LOG_LEVEL_ERROR));
    CHECK(M_ReadsAs(&reader, "ABCABC"));
    Reader_Close(&reader);
    source->funcs->close(source);
    CHECK_EQ_INT(test.live_blocks, 0);
}

TEST(a_wrong_checksum_fails)
{
    uint8_t compressed[16];
    PBG3_BUILDER_WRITER bw;
    Pbg3Builder_Init(&bw);
    Pbg3Builder_WriteLiteral(&bw, 'x');
    Pbg3Builder_FlushByte(&bw);
    memcpy(compressed, bw.bytes, bw.length);

    const PBG3_BUILDER_ENTRY entry = {
        .name = "x.txt",
        .compressed = compressed,
        .compressed_size = bw.length,
        .checksum = Pbg3Builder_Checksum(compressed, bw.length) + 1,
        .decoded_size = 1,
    };
    uint8_t archive[PBG3_BUILDER_MAX_ARCHIVE_SIZE];
    const size_t size = Pbg3Builder_BuildArchive(&entry, 1, archive);

    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    VFS_SOURCE *source;
    CHECK(Result_Log(
        M_Open(archive, size, &test.allocator, &source), LOG_LEVEL_ERROR));

    READER reader;
    const RESULT result =
        source->funcs->open(source, "x.txt", VFS_IN_MEMORY, &reader);
    CHECK(!IS_OK(result));
    CHECK(strstr(result.message, "test.dat") != nullptr);
    Result_Ignore(result);
    source->funcs->close(source);
    CHECK_EQ_INT(test.live_blocks, 0);
}

TEST(a_decoded_size_smaller_than_the_stream_produces_fails)
{
    PBG3_BUILDER_WRITER bw;
    Pbg3Builder_Init(&bw);
    Pbg3Builder_WriteLiteral(&bw, 'a');
    Pbg3Builder_WriteLiteral(&bw, 'b');
    Pbg3Builder_FlushByte(&bw);

    const PBG3_BUILDER_ENTRY entry = {
        .name = "y.txt",
        .compressed = bw.bytes,
        .compressed_size = bw.length,
        .checksum = Pbg3Builder_Checksum(bw.bytes, bw.length),
        .decoded_size = 1, // the stream has 2 literal bytes
    };
    uint8_t archive[PBG3_BUILDER_MAX_ARCHIVE_SIZE];
    const size_t size = Pbg3Builder_BuildArchive(&entry, 1, archive);

    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    VFS_SOURCE *source;
    CHECK(Result_Log(
        M_Open(archive, size, &test.allocator, &source), LOG_LEVEL_ERROR));

    READER reader;
    const RESULT result =
        source->funcs->open(source, "y.txt", VFS_IN_MEMORY, &reader);
    CHECK(!IS_OK(result));
    Result_Ignore(result);
    source->funcs->close(source);
    CHECK_EQ_INT(test.live_blocks, 0);
}

TEST(a_decoded_size_larger_than_the_stream_produces_fails)
{
    PBG3_BUILDER_WRITER bw;
    Pbg3Builder_Init(&bw);
    Pbg3Builder_WriteLiteral(&bw, 'a');
    Pbg3Builder_WriteStop(&bw);
    Pbg3Builder_FlushByte(&bw);

    const PBG3_BUILDER_ENTRY entry = {
        .name = "z.txt",
        .compressed = bw.bytes,
        .compressed_size = bw.length,
        .checksum = Pbg3Builder_Checksum(bw.bytes, bw.length),
        .decoded_size = 5, // the stream stops after 1 byte
    };
    uint8_t archive[PBG3_BUILDER_MAX_ARCHIVE_SIZE];
    const size_t size = Pbg3Builder_BuildArchive(&entry, 1, archive);

    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    VFS_SOURCE *source;
    CHECK(Result_Log(
        M_Open(archive, size, &test.allocator, &source), LOG_LEVEL_ERROR));

    READER reader;
    const RESULT result =
        source->funcs->open(source, "z.txt", VFS_IN_MEMORY, &reader);
    CHECK(!IS_OK(result));
    Result_Ignore(result);
    source->funcs->close(source);
    CHECK_EQ_INT(test.live_blocks, 0);
}

TEST(a_truncated_table_fails)
{
    PBG3_BUILDER_WRITER bw;
    Pbg3Builder_Init(&bw);
    Pbg3Builder_WriteLiteral(&bw, 'x');
    Pbg3Builder_FlushByte(&bw);

    const PBG3_BUILDER_ENTRY entry = {
        .name = "x.txt",
        .compressed = bw.bytes,
        .compressed_size = bw.length,
        .checksum = Pbg3Builder_Checksum(bw.bytes, bw.length),
        .decoded_size = 1,
    };
    uint8_t archive[PBG3_BUILDER_MAX_ARCHIVE_SIZE];
    const size_t size = Pbg3Builder_BuildArchive(&entry, 1, archive);

    VFS_SOURCE *source;
    const RESULT result = M_Open(archive, size - 2, &g_HeapAllocator, &source);
    CHECK(!IS_OK(result));
    Result_Ignore(result);
}

TEST(a_bad_magic_fails)
{
    uint8_t archive[16] = "XXXX";
    VFS_SOURCE *source;
    const RESULT result =
        M_Open(archive, sizeof(archive), &g_HeapAllocator, &source);
    CHECK(!IS_OK(result));
    Result_Ignore(result);
}

TEST(lookup_is_by_last_component_and_ignores_ascii_case)
{
    uint8_t compressed[16];
    PBG3_BUILDER_WRITER bw;
    Pbg3Builder_Init(&bw);
    Pbg3Builder_WriteLiteral(&bw, 'v');
    Pbg3Builder_FlushByte(&bw);
    memcpy(compressed, bw.bytes, bw.length);

    const PBG3_BUILDER_ENTRY entry = {
        .name = "Title00.jpg",
        .compressed = compressed,
        .compressed_size = bw.length,
        .checksum = Pbg3Builder_Checksum(compressed, bw.length),
        .decoded_size = 1,
    };
    uint8_t archive[PBG3_BUILDER_MAX_ARCHIVE_SIZE];
    const size_t size = Pbg3Builder_BuildArchive(&entry, 1, archive);

    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    VFS *const vfs = Vfs_Create(&test.allocator);
    VFS_SOURCE *source;
    CHECK(Result_Log(
        M_Open(archive, size, &test.allocator, &source), LOG_LEVEL_ERROR));
    Vfs_MountSource(vfs, source, 0);

    CHECK(Vfs_Exists(vfs, "data/title00.jpg"));
    CHECK(Vfs_Exists(vfs, "DATA\\TITLE00.JPG"));
    CHECK(!Vfs_Exists(vfs, "data/other.jpg"));
    Vfs_Destroy(vfs);
    CHECK_EQ_INT(test.live_blocks, 0);
}

TEST(a_cp932_name_is_converted_to_utf8)
{
    char *cp932_bytes;
    CHECK(Result_Log(
        Cp932_Encode(
            "\xe3\x81\x82.txt", strlen("\xe3\x81\x82.txt"), &cp932_bytes),
        LOG_LEVEL_ERROR));

    uint8_t compressed[16];
    PBG3_BUILDER_WRITER bw;
    Pbg3Builder_Init(&bw);
    Pbg3Builder_WriteLiteral(&bw, 'w');
    Pbg3Builder_FlushByte(&bw);
    memcpy(compressed, bw.bytes, bw.length);

    const PBG3_BUILDER_ENTRY entry = {
        .name = cp932_bytes,
        .compressed = compressed,
        .compressed_size = bw.length,
        .checksum = Pbg3Builder_Checksum(compressed, bw.length),
        .decoded_size = 1,
    };
    uint8_t archive[PBG3_BUILDER_MAX_ARCHIVE_SIZE];
    const size_t size = Pbg3Builder_BuildArchive(&entry, 1, archive);
    Mem_Free(cp932_bytes);

    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    VFS_SOURCE *source;
    CHECK(Result_Log(
        M_Open(archive, size, &test.allocator, &source), LOG_LEVEL_ERROR));

    READER reader;
    CHECK(Result_Log(
        source->funcs->open(source, "\xe3\x81\x82.txt", VFS_IN_MEMORY, &reader),
        LOG_LEVEL_ERROR));
    CHECK(M_ReadsAs(&reader, "w"));
    Reader_Close(&reader);
    source->funcs->close(source);
    CHECK_EQ_INT(test.live_blocks, 0);
}

typedef struct {
    char *names[2];
    int32_t count;
} VISITED_NAMES;

static void M_Visit(void *const user, const char *const path)
{
    VISITED_NAMES *const visited = (VISITED_NAMES *)user;
    visited->names[visited->count++] = Str_Dup(path);
}

TEST(listing_visits_entry_names_folded_in_table_order)
{
    char *cp932_bytes;
    CHECK(Result_Log(
        Cp932_Encode(
            "\xe3\x81\x82.txt", strlen("\xe3\x81\x82.txt"), &cp932_bytes),
        LOG_LEVEL_ERROR));

    uint8_t compressed[16];
    PBG3_BUILDER_WRITER bw;
    Pbg3Builder_Init(&bw);
    Pbg3Builder_WriteLiteral(&bw, 'v');
    Pbg3Builder_FlushByte(&bw);
    memcpy(compressed, bw.bytes, bw.length);

    const PBG3_BUILDER_ENTRY entries[2] = {
        {
            .name = "Title00.JPG",
            .compressed = compressed,
            .compressed_size = bw.length,
            .checksum = Pbg3Builder_Checksum(compressed, bw.length),
            .decoded_size = 1,
        },
        {
            .name = cp932_bytes,
            .compressed = compressed,
            .compressed_size = bw.length,
            .checksum = Pbg3Builder_Checksum(compressed, bw.length),
            .decoded_size = 1,
        },
    };
    uint8_t archive[PBG3_BUILDER_MAX_ARCHIVE_SIZE];
    const size_t size = Pbg3Builder_BuildArchive(entries, 2, archive);
    Mem_Free(cp932_bytes);

    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    VFS_SOURCE *source;
    CHECK(Result_Log(
        M_Open(archive, size, &test.allocator, &source), LOG_LEVEL_ERROR));

    VISITED_NAMES visited = { .count = 0 };
    source->funcs->list(source, M_Visit, &visited);
    CHECK_EQ_INT(visited.count, 2);
    CHECK(strcmp(visited.names[0], "title00.jpg") == 0);
    CHECK(strcmp(visited.names[1], "\xe3\x81\x82.txt") == 0);
    Mem_Free(visited.names[0]);
    Mem_Free(visited.names[1]);

    source->funcs->close(source);
    CHECK_EQ_INT(test.live_blocks, 0);
}

// A source that answers to one fixed path with raw bytes, standing in for
// a mounted archive file so Vfs_MountFile has something to open as a
// stream before handing it to Pbg3_Open.
typedef struct {
    VFS_SOURCE base;
    char *data;
    size_t size;
} RAW_FILE_SOURCE;

static bool M_RawContains(
    const VFS_SOURCE *const source, const char *const path)
{
    return strcmp(path, "archive.dat") == 0;
}

static RESULT M_RawOpen(
    VFS_SOURCE *const source, [[maybe_unused]] const char *const path,
    [[maybe_unused]] const VFS_MODE mode, READER *const out)
{
    const RAW_FILE_SOURCE *const raw = (const RAW_FILE_SOURCE *)source;
    Reader_OpenBuffer(out, raw->data, raw->size, nullptr);
    return OK;
}

static void M_RawList(
    [[maybe_unused]] const VFS_SOURCE *const source,
    [[maybe_unused]] void (*const visit)(void *user, const char *path),
    [[maybe_unused]] void *const user)
{
}

static void M_RawClose(VFS_SOURCE *const source)
{
    RAW_FILE_SOURCE *const raw = (RAW_FILE_SOURCE *)source;
    Mem_Free(raw->data);
    Mem_Free(raw);
}

static const VFS_SOURCE_FUNCS M_RawFuncs = {
    .contains = M_RawContains,
    .open = M_RawOpen,
    .list = M_RawList,
    .close = M_RawClose,
};

TEST(mounting_through_vfs_mount_file_reads_an_entry)
{
    uint8_t compressed[16];
    PBG3_BUILDER_WRITER bw;
    Pbg3Builder_Init(&bw);
    Pbg3Builder_WriteLiteral(&bw, 'q');
    Pbg3Builder_FlushByte(&bw);
    memcpy(compressed, bw.bytes, bw.length);

    const PBG3_BUILDER_ENTRY entry = {
        .name = "q.txt",
        .compressed = compressed,
        .compressed_size = bw.length,
        .checksum = Pbg3Builder_Checksum(compressed, bw.length),
        .decoded_size = 1,
    };
    uint8_t archive[PBG3_BUILDER_MAX_ARCHIVE_SIZE];
    const size_t size = Pbg3Builder_BuildArchive(&entry, 1, archive);

    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    VFS *const vfs = Vfs_Create(&test.allocator);
    RAW_FILE_SOURCE *const raw = Mem_AllocZ(sizeof(*raw));
    raw->base.funcs = &M_RawFuncs;
    raw->base.name = "archive.dat";
    raw->data = Mem_Dup(archive, size);
    raw->size = size;
    Vfs_MountSource(vfs, &raw->base, 0);

    CHECK(Result_Log(
        Vfs_MountFile(vfs, "archive.dat", Pbg3_Open, 100), LOG_LEVEL_ERROR));

    READER reader;
    CHECK(Result_Log(
        Vfs_Open(vfs, "q.txt", VFS_IN_MEMORY, &reader), LOG_LEVEL_ERROR));
    CHECK(M_ReadsAs(&reader, "q"));
    Reader_Close(&reader);
    Vfs_Destroy(vfs);
    CHECK_EQ_INT(test.live_blocks, 0);
}
