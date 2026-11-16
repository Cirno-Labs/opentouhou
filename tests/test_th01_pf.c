#include "th01/pf.h"

#include "core/base/arena.h"

#include "test.h"
#include "test_allocator.h"

#define M_DIRECTORY_SIZE 2048
#define M_ARCHIVE_SIZE 4096

static void M_WriteU32(uint8_t *const bytes, const uint32_t value)
{
    for (int32_t i = 0; i < 4; i++) {
        bytes[i] = (uint8_t)(value >> (i * 8));
    }
}

static void M_Entry(
    uint8_t *const bytes, const int32_t index, const char *const name,
    const uint8_t *const data, const uint32_t packed_size,
    const uint32_t decoded_size, const bool compressed)
{
    uint8_t *const entry = bytes + index * 32;
    const uint32_t offset = M_DIRECTORY_SIZE + index * 16;
    entry[0] = entry[1] = compressed ? 0x95 : 1;
    entry[2] = 3;
    for (size_t i = 0; name[i] != '\0'; i++) {
        entry[3 + i] = (uint8_t)~name[i];
    }
    M_WriteU32(entry + 16, packed_size);
    M_WriteU32(entry + 20, decoded_size);
    M_WriteU32(entry + 24, offset);
    for (uint32_t i = 0; i < packed_size; i++) {
        bytes[offset + i] = data[i] ^ 0x76;
    }
}

static RESULT M_Open(
    const uint8_t *const bytes, const size_t size,
    const ALLOCATOR *const allocator, VFS_SOURCE **const out)
{
    READER reader;
    Reader_OpenBuffer(&reader, bytes, size, nullptr);
    return Pf_Open(&reader, "test.dat", allocator, out);
}

static void M_CheckData(
    VFS_SOURCE *const source, const char *const name, const char *const text)
{
    READER reader;
    const RESULT result =
        source->funcs->open(source, name, VFS_STREAM, &reader);
    CHECK(Result_Log(result, LOG_LEVEL_ERROR));
    if (!IS_OK(result)) {
        return;
    }
    CHECK_EQ_INT(Reader_Size(&reader), strlen(text));
    char buffer[64] = {};
    CHECK(Reader_Size(&reader) < sizeof(buffer));
    Reader_ReadBytes(&reader, buffer, Reader_Size(&reader));
    CHECK_EQ_STR(buffer, text);
    CHECK(Result_Ignore(Reader_Result(&reader)));
    Reader_Close(&reader);
}

TEST(raw_entries_decrypt_and_readers_have_independent_lifetimes)
{
    uint8_t bytes[M_ARCHIVE_SIZE] = {};
    M_Entry(bytes, 0, "TEST.TXT", (const uint8_t *)"hello", 5, 5, false);
    TEST_ALLOCATOR allocator;
    TestAllocator_Init(&allocator);
    VFS_SOURCE *source;
    CHECK(Result_Log(
        M_Open(bytes, sizeof(bytes), &allocator.allocator, &source),
        LOG_LEVEL_ERROR));
    CHECK(source->funcs->contains(source, "test.txt"));
    CHECK(!source->funcs->contains(source, "missing.txt"));
    READER first, second;
    CHECK(Result_Ignore(
        source->funcs->open(source, "test.txt", VFS_STREAM, &first)));
    CHECK(Result_Ignore(
        source->funcs->open(source, "TEST.TXT", VFS_IN_MEMORY, &second)));
    CHECK_EQ_INT(Reader_ReadU8(&first), 'h');
    CHECK_EQ_INT(Reader_ReadU8(&first), 'e');
    CHECK_EQ_INT(Reader_ReadU8(&second), 'h');
    source->funcs->close(source);
    CHECK_EQ_INT(Reader_ReadU8(&first), 'l');
    CHECK_EQ_INT(Reader_ReadU8(&second), 'e');
    Reader_Close(&first);
    Reader_Close(&second);
    CHECK_EQ_INT(allocator.live_blocks, 0);
    CHECK_EQ_INT(allocator.size_errors, 0);
}

TEST(rle_preserves_zero_runs_continuations_and_dropped_lookahead)
{
    uint8_t bytes[M_ARCHIVE_SIZE] = {};
    const uint8_t packed[] = { 'z', 'z', 2, 'z', 1, 'y', 'y', 0, 'x' };
    M_Entry(bytes, 0, "RUN.BIN", packed, sizeof(packed), 8, true);
    VFS_SOURCE *source;
    CHECK(
        Result_Ignore(M_Open(bytes, sizeof(bytes), &g_HeapAllocator, &source)));
    M_CheckData(source, "run.bin", "zzzzzzyy");
    source->funcs->close(source);
    M_WriteU32(bytes + 20, 9);
    CHECK(
        Result_Ignore(M_Open(bytes, sizeof(bytes), &g_HeapAllocator, &source)));
    READER reader;
    CHECK(!Result_Ignore(
        source->funcs->open(source, "run.bin", VFS_STREAM, &reader)));
    CHECK(Reader_Size(&reader) == 0);
    source->funcs->close(source);
}

TEST(rle_finishes_a_run_across_the_packed_boundary_without_overwriting)
{
    uint8_t bytes[M_ARCHIVE_SIZE] = {};
    const uint8_t packed[] = { 'a', 'a' };
    M_Entry(bytes, 0, "RUN.BIN", packed, sizeof(packed), 6, true);
    bytes[M_DIRECTORY_SIZE + 2] = 250 ^ 0x76;
    VFS_SOURCE *source;
    CHECK(
        Result_Ignore(M_Open(bytes, sizeof(bytes), &g_HeapAllocator, &source)));
    M_CheckData(source, "run.bin", "aaaaaa");
    source->funcs->close(source);
}

TEST(rle_accepts_the_original_boss_ending_without_copying_adjacent_data)
{
    uint8_t bytes[M_ARCHIVE_SIZE] = {};
    const uint8_t packed[] = { 0x3f, 0xff, 0xf0, 0x00 };
    M_Entry(bytes, 0, "BOSS.BOS", packed, sizeof(packed), 4, true);
    const uint8_t following[] = { 'B', 'O', 'S', 'S', 0 };
    for (size_t i = 0; i < sizeof(following); i++) {
        bytes[M_DIRECTORY_SIZE + sizeof(packed) + i] = following[i] ^ 0x76;
    }
    VFS_SOURCE *source;
    CHECK(
        Result_Ignore(M_Open(bytes, sizeof(bytes), &g_HeapAllocator, &source)));
    READER reader;
    CHECK(Result_Ignore(
        source->funcs->open(source, "boss.bos", VFS_STREAM, &reader)));
    uint8_t decoded[4];
    Reader_ReadBytes(&reader, decoded, sizeof(decoded));
    CHECK(memcmp(decoded, packed, sizeof(packed)) == 0);
    CHECK_EQ_INT(Reader_Size(&reader), 4);
    Reader_Close(&reader);
    source->funcs->close(source);
}

TEST(rle_stops_at_the_physical_archive_end_and_releases_failed_output)
{
    uint8_t bytes[M_ARCHIVE_SIZE] = {};
    const uint8_t packed[] = { 'a', 'b' };
    M_Entry(bytes, 0, "BAD.BIN", packed, sizeof(packed), 3, true);
    TEST_ALLOCATOR allocator;
    TestAllocator_Init(&allocator);
    VFS_SOURCE *source;
    CHECK(Result_Ignore(
        M_Open(bytes, M_DIRECTORY_SIZE + 2, &allocator.allocator, &source)));
    const int32_t before = allocator.live_blocks;
    READER reader;
    CHECK(!Result_Ignore(
        source->funcs->open(source, "bad.bin", VFS_STREAM, &reader)));
    CHECK_EQ_INT(allocator.live_blocks, before);
    CHECK_EQ_INT(Reader_Size(&reader), 0);
    source->funcs->close(source);
    CHECK_EQ_INT(allocator.live_blocks, 0);
    CHECK_EQ_INT(allocator.size_errors, 0);
}

static void M_Count(void *const user, [[maybe_unused]] const char *const name)
{
    (*(int32_t *)user)++;
}

TEST(all_64_directory_entries_and_the_empty_sentinel_are_supported)
{
    uint8_t bytes[M_ARCHIVE_SIZE] = {};
    for (int32_t i = 0; i < 64; i++) {
        char name[13];
        snprintf(name, sizeof(name), "F%02d.BIN", i);
        M_Entry(bytes, i, name, (const uint8_t *)"a", 1, 1, false);
    }
    VFS_SOURCE *source;
    CHECK(
        Result_Ignore(M_Open(bytes, sizeof(bytes), &g_HeapAllocator, &source)));
    int32_t count = 0;
    source->funcs->list(source, "", M_Count, &count);
    CHECK_EQ_INT(count, 64);
    M_CheckData(source, "f63.bin", "a");
    source->funcs->close(source);
    bytes[0] = 0;
    CHECK(
        Result_Ignore(M_Open(bytes, sizeof(bytes), &g_HeapAllocator, &source)));
    count = 0;
    source->funcs->list(source, "", M_Count, &count);
    CHECK_EQ_INT(count, 0);
    source->funcs->close(source);
}

TEST(cp932_names_are_exposed_as_utf8)
{
    uint8_t bytes[M_ARCHIVE_SIZE] = {};
    M_Entry(bytes, 0, "\x82\xa0.TXT", (const uint8_t *)"text", 4, 4, false);
    VFS_SOURCE *source;
    CHECK(
        Result_Ignore(M_Open(bytes, sizeof(bytes), &g_HeapAllocator, &source)));
    M_CheckData(source, "あ.txt", "text");
    source->funcs->close(source);
}

TEST(truncated_directories_fail_without_leaks)
{
    uint8_t bytes[M_ARCHIVE_SIZE] = {};
    for (int32_t i = 0; i < 64; i++) {
        M_Entry(bytes, i, "A.BIN", (const uint8_t *)"a", 1, 1, false);
    }
    TEST_ALLOCATOR allocator;
    TestAllocator_Init(&allocator);
    for (size_t size = 0; size < M_DIRECTORY_SIZE; size++) {
        VFS_SOURCE *source;
        CHECK(
            !Result_Ignore(M_Open(bytes, size, &allocator.allocator, &source)));
        CHECK(source == nullptr);
        CHECK_EQ_INT(allocator.live_blocks, 0);
    }
    CHECK_EQ_INT(allocator.size_errors, 0);
}

TEST(compact_directories_end_at_the_sentinel_before_the_64_entry_capacity)
{
    uint8_t bytes[M_ARCHIVE_SIZE] = {};
    M_Entry(bytes, 0, "SHORT.TXT", (const uint8_t *)"a", 1, 1, false);
    M_WriteU32(bytes + 24, 64);
    bytes[64] = 'a' ^ 0x76;
    VFS_SOURCE *source;
    CHECK(Result_Ignore(M_Open(bytes, 65, &g_HeapAllocator, &source)));
    M_CheckData(source, "short.txt", "a");
    source->funcs->close(source);
    memset(bytes, 0, 32);
    CHECK(Result_Ignore(M_Open(bytes, 32, &g_HeapAllocator, &source)));
    CHECK(!source->funcs->contains(source, "short.txt"));
    source->funcs->close(source);
}

TEST(invalid_entry_ranges_counts_and_names_fail_without_leaks)
{
    TEST_ALLOCATOR allocator;
    TestAllocator_Init(&allocator);
    for (int32_t variant = 0; variant < 9; variant++) {
        uint8_t bytes[M_ARCHIVE_SIZE] = {};
        M_Entry(bytes, 0, "GOOD.BIN", (const uint8_t *)"a", 1, 1, false);
        switch (variant) {
        case 0:
            M_WriteU32(bytes + 24, 0);
            break;
        case 1:
            M_WriteU32(bytes + 24, UINT32_MAX);
            break;
        case 2:
            M_WriteU32(bytes + 16, UINT32_MAX);
            break;
        case 3:
            M_WriteU32(bytes + 20, 65536);
            break;
        case 4:
            M_WriteU32(bytes + 20, 2);
            break;
        case 5:
            memset(bytes + 3, 1, 13);
            break;
        case 6:
            bytes[3] = (uint8_t)~'/';
            break;
        case 7:
            bytes[3] = (uint8_t)~0x81;
            bytes[4] = 0;
            break;
        case 8:
            bytes[3] = 0xff;
            break;
        }
        VFS_SOURCE *source;
        CHECK(!Result_Ignore(
            M_Open(bytes, sizeof(bytes), &allocator.allocator, &source)));
        CHECK(source == nullptr);
        CHECK_EQ_INT(allocator.live_blocks, 0);
    }
    CHECK_EQ_INT(allocator.size_errors, 0);
}

TEST(opener_failure_releases_the_owned_archive_reader)
{
    TEST_ALLOCATOR allocator;
    TestAllocator_Init(&allocator);
    uint8_t *const bytes = Allocator_Alloc(&allocator.allocator, 1);
    bytes[0] = 0;
    READER archive;
    Reader_OpenOwnedBuffer(&archive, bytes, 1, &allocator.allocator, nullptr);
    VFS_SOURCE *source;
    CHECK(!Result_Ignore(
        Pf_Open(&archive, "bad.dat", &allocator.allocator, &source)));
    CHECK(source == nullptr);
    CHECK_EQ_INT(allocator.live_blocks, 0);
    CHECK_EQ_INT(allocator.size_errors, 0);
}

TEST(vfs_mounts_pf_resources_with_case_insensitive_names)
{
    uint8_t bytes[M_ARCHIVE_SIZE] = {};
    M_Entry(bytes, 0, "MOUNT.TXT", (const uint8_t *)"hello", 5, 5, false);
    TEST_ALLOCATOR allocator;
    TestAllocator_Init(&allocator);
    VFS_SOURCE *source;
    CHECK(Result_Ignore(
        M_Open(bytes, sizeof(bytes), &allocator.allocator, &source)));
    VFS *const vfs = Vfs_Create(&allocator.allocator);
    Vfs_MountSource(vfs, "", source, 100);
    READER reader;
    CHECK(Result_Ignore(Vfs_Open(vfs, "MOUNT.TXT", VFS_STREAM, &reader)));
    Vfs_Destroy(vfs);
    char buffer[6] = {};
    Reader_ReadBytes(&reader, buffer, 5);
    CHECK_EQ_STR(buffer, "hello");
    Reader_Close(&reader);
    CHECK_EQ_INT(allocator.live_blocks, 0);
    CHECK_EQ_INT(allocator.size_errors, 0);
}

TEST(missing_entries_do_not_allocate_and_duplicate_names_keep_first_match)
{
    uint8_t bytes[M_ARCHIVE_SIZE] = {};
    M_Entry(bytes, 0, "SAME.TXT", (const uint8_t *)"first", 5, 5, false);
    M_Entry(bytes, 1, "SAME.TXT", (const uint8_t *)"last", 4, 4, false);
    TEST_ALLOCATOR allocator;
    TestAllocator_Init(&allocator);
    VFS_SOURCE *source;
    CHECK(Result_Ignore(
        M_Open(bytes, sizeof(bytes), &allocator.allocator, &source)));
    READER reader;
    const int32_t before = allocator.live_blocks;
    CHECK(!Result_Ignore(
        source->funcs->open(source, "missing", VFS_STREAM, &reader)));
    CHECK_EQ_INT(allocator.live_blocks, before);
    M_CheckData(source, "same.txt", "first");
    source->funcs->close(source);
    CHECK_EQ_INT(allocator.live_blocks, 0);
}

TEST(source_and_readers_share_an_arena_until_it_is_reset)
{
    uint8_t bytes[M_ARCHIVE_SIZE] = {};
    M_Entry(bytes, 0, "FIRST.TXT", (const uint8_t *)"first", 5, 5, false);
    M_Entry(bytes, 1, "SECOND.TXT", (const uint8_t *)"second", 6, 6, false);
    ARENA arena;
    Arena_Init(&arena, "PF assets", 8192);
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

TEST(failed_directories_leave_the_caller_arena_unchanged)
{
    uint8_t bytes[M_ARCHIVE_SIZE] = {};
    M_Entry(bytes, 0, "BAD.BIN", (const uint8_t *)"a", 1, 1, false);
    M_WriteU32(bytes + 24, UINT32_MAX);
    ARENA arena;
    Arena_Init(&arena, "PF errors", 8192);
    Arena_Alloc(&arena, 7);
    const size_t before = arena.used;
    VFS_SOURCE *source;
    CHECK(!Result_Ignore(
        M_Open(bytes, sizeof(bytes), Arena_Allocator(&arena), &source)));
    CHECK(source == nullptr);
    CHECK_EQ_INT(arena.used, before);
    Arena_Free(&arena);
}

TEST(entry_storage_holds_the_longest_cp932_to_utf8_expansion)
{
    uint8_t bytes[M_ARCHIVE_SIZE] = {};
    const char name[] = "\xa1\xa1\xa1\xa1\xa1\xa1\xa1\xa1\xa1\xa1\xa1\xa1";
    M_Entry(bytes, 0, name, (const uint8_t *)"text", 4, 4, false);
    TEST_ALLOCATOR allocator;
    TestAllocator_Init(&allocator);
    VFS_SOURCE *source;
    CHECK(Result_Ignore(
        M_Open(bytes, sizeof(bytes), &allocator.allocator, &source)));
    CHECK_EQ_INT(allocator.live_blocks, 1);
    M_CheckData(source, "｡｡｡｡｡｡｡｡｡｡｡｡", "text");
    source->funcs->close(source);
    CHECK_EQ_INT(allocator.live_blocks, 0);
    CHECK_EQ_INT(allocator.size_errors, 0);
}

TEST(failure_formats_the_name_before_closing_its_owned_input)
{
    TEST_ALLOCATOR allocator;
    TestAllocator_Init(&allocator);
    const char name[] = "bad.dat";
    uint8_t *const bytes = Allocator_Alloc(&allocator.allocator, sizeof(name));
    memcpy(bytes, name, sizeof(name));
    READER archive;
    Reader_OpenOwnedBuffer(
        &archive, bytes, sizeof(name), &allocator.allocator, nullptr);
    VFS_SOURCE *source;
    RESULT result =
        Pf_Open(&archive, (const char *)bytes, &allocator.allocator, &source);
    CHECK(!IS_OK(result));
    CHECK(strstr(result.message, name) != nullptr);
    CHECK(!Result_Ignore(result));
    CHECK(source == nullptr);
    CHECK_EQ_INT(allocator.live_blocks, 0);
}
