#include "stg/pbg3.h"

#include "core/base/cp932.h"
#include "test.h"
#include "test_allocator.h"

#include <string.h>

// Builds a PBG3 archive byte for byte, to check the reader against bytes
// whose layout is known, instead of trusting a second implementation of
// the format.

#define M_HEADER_VARINT_BYTES 4
#define M_MAX_ARCHIVE_SIZE 65536

typedef struct {
    uint8_t bytes[M_MAX_ARCHIVE_SIZE];
    size_t length;
    uint32_t current;
    int32_t bits;
} BIT_WRITER;

static void M_BitWriter_Init(BIT_WRITER *const bw)
{
    bw->length = 0;
    bw->current = 0;
    bw->bits = 0;
}

static void M_WriteBits(
    BIT_WRITER *const bw, const uint32_t value, const int32_t count)
{
    for (int32_t i = count - 1; i >= 0; i--) {
        bw->current = (bw->current << 1) | ((value >> i) & 1);
        bw->bits++;
        if (bw->bits == 8) {
            bw->bytes[bw->length++] = (uint8_t)bw->current;
            bw->current = 0;
            bw->bits = 0;
        }
    }
}

static void M_FlushByte(BIT_WRITER *const bw)
{
    if (bw->bits > 0) {
        bw->bytes[bw->length++] = (uint8_t)(bw->current << (8 - bw->bits));
        bw->current = 0;
        bw->bits = 0;
    }
}

// Writes a varint using exactly byte_count bytes, so the caller can place a
// value whose encoded size is known ahead of what the value turns out to
// be, such as the header's table offset.
static void M_WriteVarintN(
    BIT_WRITER *const bw, const uint32_t value, const int32_t byte_count)
{
    M_WriteBits(bw, (uint32_t)(byte_count - 1), 2);
    M_WriteBits(bw, value, byte_count * 8);
}

static void M_WriteVarint(BIT_WRITER *const bw, const uint32_t value)
{
    int32_t byte_count = 1;
    while (byte_count < 4 && (value >> (byte_count * 8)) != 0) {
        byte_count++;
    }
    M_WriteVarintN(bw, value, byte_count);
}

static void M_WriteName(BIT_WRITER *const bw, const char *const name)
{
    for (const unsigned char *c = (const unsigned char *)name; *c != '\0';
         c++) {
        M_WriteBits(bw, *c, 8);
    }
    M_WriteBits(bw, 0, 8);
}

static void M_WriteLiteral(BIT_WRITER *const bw, const uint8_t byte)
{
    M_WriteBits(bw, 1, 1);
    M_WriteBits(bw, byte, 8);
}

// Writes a back-reference copying length bytes (3 to 18) starting at dict
// index offset. dict[k] holds the (k - 1)th byte this stream has produced,
// so offset is one more than the index of the byte to repeat.
static void M_WriteBackref(
    BIT_WRITER *const bw, const uint32_t offset, const uint32_t length)
{
    M_WriteBits(bw, 0, 1);
    M_WriteBits(bw, offset, 13);
    M_WriteBits(bw, length - 3, 4);
}

static void M_WriteStop(BIT_WRITER *const bw)
{
    M_WriteBits(bw, 0, 1);
    M_WriteBits(bw, 0, 13);
}

static uint32_t M_Checksum(const uint8_t *const bytes, const size_t size)
{
    uint32_t sum = 0;
    for (size_t i = 0; i < size; i++) {
        sum += bytes[i];
    }
    return sum;
}

typedef struct {
    const char *name;
    const uint8_t *compressed;
    size_t compressed_size;
    uint32_t checksum;
    uint32_t decoded_size;
} BUILD_ENTRY;

// Assembles a whole PBG3 archive from already-compressed entries: the
// magic, a fixed-width header, each entry's compressed bytes back to back,
// and the table. Returns the number of bytes written.
static size_t M_BuildArchive(
    const BUILD_ENTRY *const entries, const int32_t count, uint8_t *const out)
{
    const size_t header_bits = (2 + M_HEADER_VARINT_BYTES * 8) * 2;
    const size_t header_size = (header_bits + 7) / 8;
    const size_t data_start = 4 + header_size;

    size_t offsets[64];
    size_t cursor = data_start;
    for (int32_t i = 0; i < count; i++) {
        offsets[i] = cursor;
        cursor += entries[i].compressed_size;
    }
    const uint32_t table_offset = (uint32_t)cursor;

    memcpy(out, "PBG3", 4);
    BIT_WRITER header;
    M_BitWriter_Init(&header);
    M_WriteVarintN(&header, (uint32_t)count, M_HEADER_VARINT_BYTES);
    M_WriteVarintN(&header, table_offset, M_HEADER_VARINT_BYTES);
    M_FlushByte(&header);
    CHECK_EQ_INT(header.length, header_size);
    memcpy(out + 4, header.bytes, header.length);

    for (int32_t i = 0; i < count; i++) {
        memcpy(
            out + offsets[i], entries[i].compressed,
            entries[i].compressed_size);
    }

    BIT_WRITER table;
    M_BitWriter_Init(&table);
    for (int32_t i = 0; i < count; i++) {
        M_WriteVarint(&table, 0); // unk1
        M_WriteVarint(&table, 0); // unk0
        M_WriteVarint(&table, entries[i].checksum);
        M_WriteVarint(&table, (uint32_t)offsets[i]);
        M_WriteVarint(&table, entries[i].decoded_size);
        M_WriteName(&table, entries[i].name);
    }
    M_FlushByte(&table);
    memcpy(out + table_offset, table.bytes, table.length);

    return table_offset + table.length;
}

static RESULT M_Open(
    const uint8_t *const bytes, const size_t size,
    const ALLOCATOR *const allocator, VFS_SOURCE **const out)
{
    READER *const reader = Mem_Alloc(sizeof(*reader));
    Reader_OpenBuffer(reader, bytes, size);
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
    BIT_WRITER bw;
    M_BitWriter_Init(&bw);
    const char *const text = "hello";
    for (const char *c = text; *c != '\0'; c++) {
        M_WriteLiteral(&bw, (uint8_t)*c);
    }
    M_FlushByte(&bw);
    memcpy(compressed, bw.bytes, bw.length);

    const BUILD_ENTRY entry = {
        .name = "hello.txt",
        .compressed = compressed,
        .compressed_size = bw.length,
        .checksum = M_Checksum(compressed, bw.length),
        .decoded_size = 5,
    };
    uint8_t archive[M_MAX_ARCHIVE_SIZE];
    const size_t size = M_BuildArchive(&entry, 1, archive);

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
    BIT_WRITER bw;
    M_BitWriter_Init(&bw);
    M_WriteLiteral(&bw, 'A');
    M_WriteBackref(&bw, 1, 8); // repeat dict[1] eight times, self-overlapping
    M_FlushByte(&bw);

    const BUILD_ENTRY entry = {
        .name = "a.txt",
        .compressed = bw.bytes,
        .compressed_size = bw.length,
        .checksum = M_Checksum(bw.bytes, bw.length),
        .decoded_size = 9,
    };
    uint8_t archive[M_MAX_ARCHIVE_SIZE];
    const size_t size = M_BuildArchive(&entry, 1, archive);

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
    BIT_WRITER bw;
    M_BitWriter_Init(&bw);
    M_WriteLiteral(&bw, 'A');
    M_WriteLiteral(&bw, 'B');
    M_WriteLiteral(&bw, 'C');
    M_WriteBackref(&bw, 1, 3); // repeat dict[1..3] once: "ABC"
    M_FlushByte(&bw);

    const BUILD_ENTRY entry = {
        .name = "b.txt",
        .compressed = bw.bytes,
        .compressed_size = bw.length,
        .checksum = M_Checksum(bw.bytes, bw.length),
        .decoded_size = 6,
    };
    uint8_t archive[M_MAX_ARCHIVE_SIZE];
    const size_t size = M_BuildArchive(&entry, 1, archive);

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
    BIT_WRITER bw;
    M_BitWriter_Init(&bw);
    M_WriteLiteral(&bw, 'x');
    M_FlushByte(&bw);
    memcpy(compressed, bw.bytes, bw.length);

    const BUILD_ENTRY entry = {
        .name = "x.txt",
        .compressed = compressed,
        .compressed_size = bw.length,
        .checksum = M_Checksum(compressed, bw.length) + 1,
        .decoded_size = 1,
    };
    uint8_t archive[M_MAX_ARCHIVE_SIZE];
    const size_t size = M_BuildArchive(&entry, 1, archive);

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
    BIT_WRITER bw;
    M_BitWriter_Init(&bw);
    M_WriteLiteral(&bw, 'a');
    M_WriteLiteral(&bw, 'b');
    M_FlushByte(&bw);

    const BUILD_ENTRY entry = {
        .name = "y.txt",
        .compressed = bw.bytes,
        .compressed_size = bw.length,
        .checksum = M_Checksum(bw.bytes, bw.length),
        .decoded_size = 1, // the stream has 2 literal bytes
    };
    uint8_t archive[M_MAX_ARCHIVE_SIZE];
    const size_t size = M_BuildArchive(&entry, 1, archive);

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
    BIT_WRITER bw;
    M_BitWriter_Init(&bw);
    M_WriteLiteral(&bw, 'a');
    M_WriteStop(&bw);
    M_FlushByte(&bw);

    const BUILD_ENTRY entry = {
        .name = "z.txt",
        .compressed = bw.bytes,
        .compressed_size = bw.length,
        .checksum = M_Checksum(bw.bytes, bw.length),
        .decoded_size = 5, // the stream stops after 1 byte
    };
    uint8_t archive[M_MAX_ARCHIVE_SIZE];
    const size_t size = M_BuildArchive(&entry, 1, archive);

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
    BIT_WRITER bw;
    M_BitWriter_Init(&bw);
    M_WriteLiteral(&bw, 'x');
    M_FlushByte(&bw);

    const BUILD_ENTRY entry = {
        .name = "x.txt",
        .compressed = bw.bytes,
        .compressed_size = bw.length,
        .checksum = M_Checksum(bw.bytes, bw.length),
        .decoded_size = 1,
    };
    uint8_t archive[M_MAX_ARCHIVE_SIZE];
    const size_t size = M_BuildArchive(&entry, 1, archive);

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
    BIT_WRITER bw;
    M_BitWriter_Init(&bw);
    M_WriteLiteral(&bw, 'v');
    M_FlushByte(&bw);
    memcpy(compressed, bw.bytes, bw.length);

    const BUILD_ENTRY entry = {
        .name = "Title00.jpg",
        .compressed = compressed,
        .compressed_size = bw.length,
        .checksum = M_Checksum(compressed, bw.length),
        .decoded_size = 1,
    };
    uint8_t archive[M_MAX_ARCHIVE_SIZE];
    const size_t size = M_BuildArchive(&entry, 1, archive);

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
    BIT_WRITER bw;
    M_BitWriter_Init(&bw);
    M_WriteLiteral(&bw, 'w');
    M_FlushByte(&bw);
    memcpy(compressed, bw.bytes, bw.length);

    const BUILD_ENTRY entry = {
        .name = cp932_bytes,
        .compressed = compressed,
        .compressed_size = bw.length,
        .checksum = M_Checksum(compressed, bw.length),
        .decoded_size = 1,
    };
    uint8_t archive[M_MAX_ARCHIVE_SIZE];
    const size_t size = M_BuildArchive(&entry, 1, archive);
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
    Reader_OpenBuffer(out, raw->data, raw->size);
    return OK;
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
    .close = M_RawClose,
};

TEST(mounting_through_vfs_mount_file_reads_an_entry)
{
    uint8_t compressed[16];
    BIT_WRITER bw;
    M_BitWriter_Init(&bw);
    M_WriteLiteral(&bw, 'q');
    M_FlushByte(&bw);
    memcpy(compressed, bw.bytes, bw.length);

    const BUILD_ENTRY entry = {
        .name = "q.txt",
        .compressed = compressed,
        .compressed_size = bw.length,
        .checksum = M_Checksum(compressed, bw.length),
        .decoded_size = 1,
    };
    uint8_t archive[M_MAX_ARCHIVE_SIZE];
    const size_t size = M_BuildArchive(&entry, 1, archive);

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
