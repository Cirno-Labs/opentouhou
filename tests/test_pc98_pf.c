#include "pc98/zun/pf.h"

#include "core/base/arena.h"

#include "test.h"
#include "test_allocator.h"

#define M_DATA_OFFSET 1024
#define M_ARCHIVE_SIZE 4096

static void M_U32(uint8_t *const bytes, const uint32_t value)
{
    for (size_t i = 0; i < 4; i++) {
        bytes[i] = (uint8_t)(value >> (i * 8));
    }
}

static void M_Entry(
    uint8_t *const bytes, const size_t slot, const char *const name,
    const uint32_t offset, const uint8_t *const data,
    const uint32_t packed_size, const uint32_t size, const uint16_t type)
{
    uint8_t *const entry = bytes + slot * 32;
    entry[0] = (uint8_t)type;
    entry[1] = (uint8_t)(type >> 8);
    entry[2] = 0xa5;
    for (size_t i = 0; name[i] != '\0'; i++) {
        entry[3 + i] = (uint8_t)~(uint8_t)name[i];
    }
    M_U32(entry + 16, packed_size);
    M_U32(entry + 20, size);
    M_U32(entry + 24, offset);
    for (size_t i = 0; i < packed_size; i++) {
        bytes[offset + i] = data[i] ^ 0x76;
    }
}

static RESULT M_Open(
    const void *const bytes, const size_t size,
    const ALLOCATOR *const allocator, VFS_SOURCE **const source)
{
    READER reader;
    Reader_OpenBuffer(&reader, bytes, size, "synthetic.pf");
    return Pf_Open(&reader, PF_TH01, "synthetic.pf", allocator, source);
}

static void M_CheckContent(
    VFS *const vfs, const char *const name, const void *const expected,
    const size_t size)
{
    READER reader;
    const bool opened =
        Result_Log(Vfs_Open(vfs, name, VFS_STREAM, &reader), LOG_LEVEL_ERROR);
    CHECK(opened);
    if (opened) {
        CHECK_EQ_INT(Reader_Size(&reader), size);
        CHECK(memcmp(Reader_Borrow(&reader, size), expected, size) == 0);
        CHECK(Result_Ignore(Reader_Result(&reader)));
        Reader_Close(&reader);
    }
}

TEST(raw_entries_use_utf8_names_casefolding_and_independent_readers)
{
    uint8_t bytes[M_ARCHIVE_SIZE] = {};
    M_Entry(
        bytes, 0, "TEST.TXT", M_DATA_OFFSET, (const uint8_t *)"hello", 5, 5,
        0xf388);
    M_Entry(
        bytes, 1, "\x82\xa0.TXT", M_DATA_OFFSET + 32, (const uint8_t *)"kana",
        4, 4, 0xf388);
    TEST_ALLOCATOR allocator;
    TestAllocator_Init(&allocator);
    VFS_SOURCE *source;
    const bool loaded = Result_Ignore(
        M_Open(bytes, sizeof(bytes), &allocator.allocator, &source));
    CHECK(loaded);
    if (!loaded) {
        return;
    }
    CHECK(source->funcs->write == nullptr);
    VFS *const vfs = Vfs_Create(&g_HeapAllocator);
    Vfs_MountSource(vfs, "", source, 0);
    M_CheckContent(vfs, "TEST.txt", "hello", 5);
    M_CheckContent(vfs, "あ.txt", "kana", 4);
    READER first, second;
    const bool first_opened =
        Result_Ignore(Vfs_Open(vfs, "test.txt", VFS_IN_MEMORY, &first));
    const bool second_opened =
        Result_Ignore(Vfs_Open(vfs, "test.txt", VFS_STREAM, &second));
    CHECK(first_opened && second_opened);
    READER missing;
    CHECK(!Result_Ignore(Vfs_Open(vfs, "dir/test.txt", VFS_STREAM, &missing)));
    Vfs_Destroy(vfs);
    if (first_opened && second_opened) {
        CHECK_EQ_INT(Reader_ReadU8(&first), 'h');
        CHECK_EQ_INT(Reader_ReadU8(&first), 'e');
        CHECK_EQ_INT(Reader_ReadU8(&second), 'h');
    }
    if (first_opened) {
        Reader_Close(&first);
    }
    if (second_opened) {
        Reader_Close(&second);
    }
    CHECK_EQ_INT(allocator.live_blocks, 0);
    CHECK_EQ_INT(allocator.size_errors, 0);
}

TEST(contents_use_the_key_of_the_archive)
{
    uint8_t bytes[M_ARCHIVE_SIZE] = {};
    const uint8_t packed[] = { 'a', 'a', 2, 'b' };
    M_Entry(
        bytes, 0, "RAW.TXT", M_DATA_OFFSET, (const uint8_t *)"hello", 5, 5,
        0xf388);
    M_Entry(
        bytes, 1, "RUN.BIN", M_DATA_OFFSET + 32, packed, sizeof(packed), 5,
        0x9595);
    READER reader;
    Reader_OpenBuffer(&reader, bytes, sizeof(bytes), "th01.pf");
    VFS_SOURCE *source;
    const bool loaded = Result_Ignore(
        Pf_Open(&reader, PF_TH01, "th01.pf", &g_HeapAllocator, &source));
    CHECK(loaded);
    if (loaded) {
        VFS *const vfs = Vfs_Create(&g_HeapAllocator);
        Vfs_MountSource(vfs, "", source, 0);
        M_CheckContent(vfs, "raw.txt", "hello", 5);
        M_CheckContent(vfs, "run.bin", "aaaab", 5);
        Vfs_Destroy(vfs);
    }
}

TEST(rle_preserves_continuations_zero_counts_and_the_last_literal)
{
    uint8_t bytes[M_ARCHIVE_SIZE] = {};
    const uint8_t packed[] = { 'a', 'a', 2, 'a', 1, 'b', 'b', 0, 'c' };
    M_Entry(
        bytes, 0, "RUN.BIN", M_DATA_OFFSET, packed, sizeof(packed), 9, 0x9595);
    VFS_SOURCE *source;
    const bool loaded =
        Result_Ignore(M_Open(bytes, sizeof(bytes), &g_HeapAllocator, &source));
    CHECK(loaded);
    if (loaded) {
        VFS *const vfs = Vfs_Create(&g_HeapAllocator);
        Vfs_MountSource(vfs, "", source, 0);
        M_CheckContent(vfs, "run.bin", "aaaaaabbc", 9);
        Vfs_Destroy(vfs);
    }
}

TEST(rle_accepts_255_counts_and_a_final_pair_without_a_count)
{
    uint8_t bytes[M_ARCHIVE_SIZE] = {};
    const uint8_t packed[] = { 'x', 'x', 255, 'x', 255 };
    const uint8_t tail[] = { 0xff, 0xff };
    M_Entry(
        bytes, 0, "LONG.BIN", M_DATA_OFFSET, packed, sizeof(packed), 513,
        0x9595);
    M_Entry(
        bytes, 1, "TAIL.BIN", M_DATA_OFFSET + 32, tail, sizeof(tail), 2,
        0x9595);
    M_Entry(bytes, 2, "EMPTY.BIN", M_DATA_OFFSET + 64, nullptr, 0, 0, 0x9595);
    VFS_SOURCE *source;
    const bool loaded =
        Result_Ignore(M_Open(bytes, sizeof(bytes), &g_HeapAllocator, &source));
    CHECK(loaded);
    if (loaded) {
        VFS *const vfs = Vfs_Create(&g_HeapAllocator);
        Vfs_MountSource(vfs, "", source, 0);
        uint8_t expected[513];
        memset(expected, 'x', sizeof(expected));
        M_CheckContent(vfs, "long.bin", expected, sizeof(expected));
        M_CheckContent(vfs, "tail.bin", tail, sizeof(tail));
        M_CheckContent(vfs, "empty.bin", "", 0);
        Vfs_Destroy(vfs);
    }
}

TEST(rle_does_not_read_a_neighboring_entry_or_leak_failed_output)
{
    uint8_t bytes[M_ARCHIVE_SIZE] = {};
    const uint8_t packed[] = { 'a', 'a' };
    M_Entry(
        bytes, 0, "SHORT.BIN", M_DATA_OFFSET, packed, sizeof(packed), 6,
        0x9595);
    bytes[M_DATA_OFFSET + 2] = 250 ^ 0x76;
    TEST_ALLOCATOR allocator;
    TestAllocator_Init(&allocator);
    VFS_SOURCE *source;
    const bool loaded = Result_Ignore(
        M_Open(bytes, sizeof(bytes), &allocator.allocator, &source));
    CHECK(loaded);
    if (!loaded) {
        return;
    }
    const int32_t before = allocator.live_blocks;
    READER reader;
    CHECK(!Result_Ignore(
        source->funcs->open(source, "short.bin", VFS_STREAM, &reader)));
    CHECK_EQ_INT(allocator.live_blocks, before);
    source->funcs->close(source);
    CHECK_EQ_INT(allocator.live_blocks, 0);
    CHECK_EQ_INT(allocator.size_errors, 0);
}

TEST(invalid_directories_release_all_allocations)
{
    const size_t offsets[] = { 0, 1, 3, 3, 3, 3, 3, 3, 16, 20, 24 };
    const uint32_t values[] = { 1,
                                1,
                                0,
                                0xff,
                                (uint8_t)~'/',
                                (uint8_t)~'\\',
                                (uint8_t)~':',
                                (uint8_t)~'.',
                                4096,
                                4096,
                                0 };
    for (size_t i = 0; i < sizeof(offsets) / sizeof(offsets[0]); i++) {
        uint8_t bytes[M_ARCHIVE_SIZE] = {};
        M_Entry(
            bytes, 0, "A", M_DATA_OFFSET, (const uint8_t *)"hello", 5, 5,
            0xf388);
        if (offsets[i] >= 16) {
            M_U32(bytes + offsets[i], values[i]);
        } else {
            bytes[offsets[i]] = (uint8_t)values[i];
        }
        TEST_ALLOCATOR allocator;
        TestAllocator_Init(&allocator);
        VFS_SOURCE *source;
        CHECK(!Result_Ignore(
            M_Open(bytes, sizeof(bytes), &allocator.allocator, &source)));
        CHECK(source == nullptr);
        CHECK_EQ_INT(allocator.live_blocks, 0);
        CHECK_EQ_INT(allocator.size_errors, 0);
    }
}

TEST(truncated_inputs_are_rejected_without_leaks)
{
    uint8_t bytes[M_ARCHIVE_SIZE] = {};
    M_Entry(
        bytes, 0, "A", M_DATA_OFFSET, (const uint8_t *)"hello", 5, 5, 0xf388);
    for (size_t size = 0; size < M_DATA_OFFSET + 5; size++) {
        TEST_ALLOCATOR allocator;
        TestAllocator_Init(&allocator);
        VFS_SOURCE *source;
        CHECK(
            !Result_Ignore(M_Open(bytes, size, &allocator.allocator, &source)));
        CHECK(source == nullptr);
        CHECK_EQ_INT(allocator.live_blocks, 0);
        CHECK_EQ_INT(allocator.size_errors, 0);
    }
}

TEST(empty_tables_are_valid_and_impossible_rle_sizes_are_rejected)
{
    uint8_t bytes[M_ARCHIVE_SIZE] = {};
    VFS_SOURCE *source;
    const bool loaded =
        Result_Ignore(M_Open(bytes, 32, &g_HeapAllocator, &source));
    CHECK(loaded);
    if (loaded) {
        CHECK(!source->funcs->contains(source, "missing"));
        source->funcs->close(source);
    }
    M_Entry(bytes, 0, "A", M_DATA_OFFSET, (const uint8_t *)"x", 1, 129, 0x9595);
    CHECK(!Result_Ignore(
        M_Open(bytes, sizeof(bytes), &g_HeapAllocator, &source)));
    CHECK(source == nullptr);
}

TEST(sizes_must_fit_the_original_16_bit_byte_counts)
{
    uint8_t bytes[M_ARCHIVE_SIZE] = {};
    M_Entry(bytes, 0, "A", M_DATA_OFFSET, nullptr, 0, 65536, 0x9595);
    M_U32(bytes + 16, 1024);
    TEST_ALLOCATOR allocator;
    TestAllocator_Init(&allocator);
    VFS_SOURCE *source;
    CHECK(!Result_Ignore(
        M_Open(bytes, sizeof(bytes), &allocator.allocator, &source)));
    CHECK(source == nullptr);
    CHECK_EQ_INT(allocator.live_blocks, 0);
    M_U32(bytes + 20, 65535);
    CHECK(Result_Ignore(
        M_Open(bytes, sizeof(bytes), &allocator.allocator, &source)));
    source->funcs->close(source);
    CHECK_EQ_INT(allocator.live_blocks, 0);
    CHECK_EQ_INT(allocator.size_errors, 0);
}

TEST(unterminated_names_and_invalid_cp932_are_rejected)
{
    VFS_SOURCE *source;
    for (size_t i = 0; i < 2; i++) {
        uint8_t bytes[M_ARCHIVE_SIZE] = {};
        M_Entry(
            bytes, 0, "A", M_DATA_OFFSET, (const uint8_t *)"x", 1, 1, 0xf388);
        if (i == 0) {
            memset(bytes + 3, (uint8_t)~'a', 13);
        } else {
            bytes[3] = (uint8_t)~0x81;
        }
        CHECK(!Result_Ignore(
            M_Open(bytes, sizeof(bytes), &g_HeapAllocator, &source)));
        CHECK(source == nullptr);
    }
}

TEST(duplicate_names_find_the_first_entry)
{
    uint8_t bytes[M_ARCHIVE_SIZE] = {};
    M_Entry(
        bytes, 0, "A.TXT", M_DATA_OFFSET, (const uint8_t *)"first", 5, 5,
        0xf388);
    M_Entry(
        bytes, 1, "a.txt", M_DATA_OFFSET + 32, (const uint8_t *)"second", 6, 6,
        0xf388);
    VFS_SOURCE *source;
    const bool loaded =
        Result_Ignore(M_Open(bytes, sizeof(bytes), &g_HeapAllocator, &source));
    CHECK(loaded);
    if (loaded) {
        VFS *const vfs = Vfs_Create(&g_HeapAllocator);
        Vfs_MountSource(vfs, "", source, 0);
        M_CheckContent(vfs, "a.txt", "first", 5);
        Vfs_Destroy(vfs);
    }
}

TEST(names_hold_the_longest_cp932_to_utf8_expansion)
{
    uint8_t bytes[M_ARCHIVE_SIZE] = {};
    M_Entry(
        bytes, 0, "\xa1\xa1\xa1\xa1\xa1\xa1\xa1\xa1\xa1\xa1\xa1\xa1",
        M_DATA_OFFSET, (const uint8_t *)"text", 4, 4, 0xf388);
    VFS_SOURCE *source;
    const bool loaded =
        Result_Ignore(M_Open(bytes, sizeof(bytes), &g_HeapAllocator, &source));
    CHECK(loaded);
    if (loaded) {
        VFS *const vfs = Vfs_Create(&g_HeapAllocator);
        Vfs_MountSource(vfs, "", source, 0);
        M_CheckContent(vfs, "｡｡｡｡｡｡｡｡｡｡｡｡", "text", 4);
        Vfs_Destroy(vfs);
    }
}

TEST(failure_closes_the_owned_archive_reader)
{
    TEST_ALLOCATOR allocator;
    TestAllocator_Init(&allocator);
    uint8_t *const bytes = Allocator_Alloc(&allocator.allocator, 1);
    bytes[0] = 0;
    READER archive;
    Reader_OpenOwnedBuffer(&archive, bytes, 1, &allocator.allocator, nullptr);
    VFS_SOURCE *source;
    const RESULT result =
        Pf_Open(&archive, PF_TH01, "bad.pf", &allocator.allocator, &source);
    CHECK(!IS_OK(result));
    CHECK(strstr(result.message, "bad.pf") != nullptr);
    CHECK(!Result_Ignore(result));
    CHECK(source == nullptr);
    CHECK_EQ_INT(allocator.live_blocks, 0);
    CHECK_EQ_INT(allocator.size_errors, 0);
}

TEST(source_and_readers_share_an_arena_until_it_is_reset)
{
    uint8_t bytes[M_ARCHIVE_SIZE] = {};
    M_Entry(
        bytes, 0, "FIRST.TXT", M_DATA_OFFSET, (const uint8_t *)"first", 5, 5,
        0xf388);
    M_Entry(
        bytes, 1, "SECOND.TXT", M_DATA_OFFSET + 32, (const uint8_t *)"second",
        6, 6, 0xf388);
    ARENA arena;
    Arena_Init(&arena, "PF assets", 16384);
    VFS_SOURCE *source;
    CHECK(Result_Ignore(
        M_Open(bytes, sizeof(bytes), Arena_Allocator(&arena), &source)));
    READER first, second;
    CHECK(Result_Ignore(
        source->funcs->open(source, "first.txt", VFS_STREAM, &first)));
    CHECK(Result_Ignore(
        source->funcs->open(source, "second.txt", VFS_STREAM, &second)));
    source->funcs->close(source);
    CHECK_EQ_INT(Reader_ReadU8(&first), 'f');
    CHECK_EQ_INT(Reader_ReadU8(&second), 's');
    Reader_Close(&first);
    Reader_Close(&second);
    CHECK(arena.used > 0);
    Arena_Reset(&arena);
    CHECK_EQ_INT(arena.used, 0);
    Arena_Free(&arena);
}

static void M_Count(void *const user, [[maybe_unused]] const char *const name)
{
    (*(size_t *)user)++;
}

TEST(the_original_seek_counter_limits_lookup_to_254_entries)
{
    const uint32_t payload = 255 * 32;
    const size_t size = payload + 255;
    uint8_t *const bytes = Mem_AllocZ(size);
    for (size_t i = 0; i < 255; i++) {
        char name[13];
        snprintf(name, sizeof(name), "F%03zu.BIN", i);
        M_Entry(
            bytes, i, name, payload + (uint32_t)i, (const uint8_t *)"x", 1, 1,
            0xf388);
    }
    VFS_SOURCE *source;
    const bool loaded =
        Result_Ignore(M_Open(bytes, size, &g_HeapAllocator, &source));
    CHECK(loaded);
    if (loaded) {
        size_t count = 0;
        source->funcs->list(source, "", M_Count, &count);
        CHECK_EQ_INT(count, 254);
        CHECK(source->funcs->contains(source, "f253.bin"));
        CHECK(!source->funcs->contains(source, "f254.bin"));
        source->funcs->close(source);
    }
    Mem_Free(bytes);
}
