#include "core/archive.h"

#include "core/archive/format.h"
#include "core/base/string.h"
#include "core/vfs.h"
#include "test.h"
#include "test_allocator.h"

#include <string.h>

// A format made up for these tests: a u32 entry count, then for each entry
// a u8 name length, the name, the u32 size, u32 mode and u32 offset of its
// data, and a u8 check value, the wrapping sum of its data bytes.

#define M_MAX_ENTRIES 4
#define M_MAX_ARCHIVE_SIZE 512

typedef struct {
    uint32_t offset;
    uint8_t check;
} M_RECORD;

typedef struct {
    const char *name;
    const char *data;
    uint32_t mode;
} M_ENTRY;

typedef struct {
    uint8_t bytes[M_MAX_ARCHIVE_SIZE];
    size_t size;
} M_ARCHIVE;

static RESULT M_ReadTable(READER *const archive, ARCHIVE_TABLE *const table)
{
    const uint32_t count = Reader_ReadU32(archive);
    FAIL_IF(count > M_MAX_ENTRIES, "too many entries");
    for (uint32_t i = 0; i < count; i++) {
        char name[256];
        const uint8_t length = Reader_ReadU8(archive);
        Reader_ReadBytes(archive, name, length);
        name[length] = '\0';
        const uint32_t size = Reader_ReadU32(archive);
        const uint32_t mode = Reader_ReadU32(archive);
        M_RECORD record = { .offset = Reader_ReadU32(archive) };
        record.check = Reader_ReadU8(archive);
        FAIL_IF(Reader_Failed(archive), "table runs past the end");
        ArchiveTable_Add(table, name, size, mode, &record, sizeof(record));
    }
    return OK;
}

static RESULT M_ReadEntry(
    READER *const archive, const void *const data, uint8_t *const out,
    const size_t size)
{
    const M_RECORD *const record = data;
    Reader_Seek(archive, record->offset);
    Reader_ReadBytes(archive, out, size);
    MUST(Reader_Result(archive));
    uint8_t check = 0;
    for (size_t i = 0; i < size; i++) {
        check = (uint8_t)(check + out[i]);
    }
    FAIL_IF(check != record->check, "check value mismatch");
    return OK;
}

static const ARCHIVE_FORMAT M_PathFormat = {
    .lookup = ARCHIVE_LOOKUP_PATH,
    .read_table = M_ReadTable,
    .read_entry = M_ReadEntry,
};

static const ARCHIVE_FORMAT M_LastNocaseFormat = {
    .lookup = ARCHIVE_LOOKUP_LAST_NOCASE,
    .read_table = M_ReadTable,
    .read_entry = M_ReadEntry,
};

static void M_Put8(M_ARCHIVE *const archive, const uint8_t value)
{
    archive->bytes[archive->size++] = value;
}

static void M_Put32(M_ARCHIVE *const archive, const uint32_t value)
{
    for (int32_t i = 0; i < 4; i++) {
        M_Put8(archive, (uint8_t)(value >> (8 * i)));
    }
}

static M_ARCHIVE M_Build(const M_ENTRY *const entries, const int32_t count)
{
    M_ARCHIVE archive = { .size = 0 };
    size_t offset = 4;
    for (int32_t i = 0; i < count; i++) {
        offset += 1 + strlen(entries[i].name) + 13;
    }
    M_Put32(&archive, (uint32_t)count);
    for (int32_t i = 0; i < count; i++) {
        const M_ENTRY *const entry = &entries[i];
        const size_t size = strlen(entry->data);
        M_Put8(&archive, (uint8_t)strlen(entry->name));
        for (const char *c = entry->name; *c != '\0'; c++) {
            M_Put8(&archive, (uint8_t)*c);
        }
        M_Put32(&archive, (uint32_t)size);
        M_Put32(&archive, entry->mode);
        M_Put32(&archive, (uint32_t)offset);
        uint8_t check = 0;
        for (size_t k = 0; k < size; k++) {
            check = (uint8_t)(check + (uint8_t)entry->data[k]);
        }
        M_Put8(&archive, check);
        offset += size;
    }
    for (int32_t i = 0; i < count; i++) {
        for (const char *c = entries[i].data; *c != '\0'; c++) {
            M_Put8(&archive, (uint8_t)*c);
        }
    }
    return archive;
}

// Opens the bytes of built through a reader named name, maybe nullptr.
static RESULT M_Open(
    const M_ARCHIVE *const built, const char *const name,
    const ARCHIVE_FORMAT *const format, const ALLOCATOR *const allocator,
    ARCHIVE **const out)
{
    READER reader;
    Reader_OpenBuffer(&reader, built->bytes, built->size, name);
    return Archive_Open(format, &reader, allocator, out);
}

static void M_CheckReads(
    ARCHIVE *const archive, const ARCHIVE_ENTRY *const entry,
    const char *const expected, const ALLOCATOR *const allocator)
{
    CHECK(entry != nullptr);
    if (entry == nullptr) {
        return;
    }
    CHECK_EQ_INT(ArchiveEntry_GetSize(entry), strlen(expected));
    uint8_t *data;
    CHECK(Result_Log(Archive_Read(archive, entry, &data), LOG_LEVEL_ERROR));
    CHECK(data != nullptr && memcmp(data, expected, strlen(expected)) == 0);
    Allocator_Free(allocator, data, ArchiveEntry_GetSize(entry));
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

TEST(entries_keep_their_names_sizes_modes_and_records)
{
    const M_ENTRY entries[] = {
        { "first.txt", "hello", 0100644 },
        { "dir/", "", 040755 },
        { "second.bin", "archive", 0 },
    };
    const M_ARCHIVE built = M_Build(entries, 3);
    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    ARCHIVE *archive;
    CHECK(Result_Log(
        M_Open(&built, "test.arc", &M_PathFormat, &test.allocator, &archive),
        LOG_LEVEL_ERROR));

    CHECK_EQ_INT(Archive_GetCount(archive), 3);
    CHECK_EQ_STR(
        ArchiveEntry_GetName(Archive_GetEntry(archive, 0)), "first.txt");
    CHECK_EQ_INT(ArchiveEntry_GetMode(Archive_GetEntry(archive, 0)), 0100644);
    CHECK_EQ_STR(ArchiveEntry_GetName(Archive_GetEntry(archive, 1)), "dir/");
    CHECK_EQ_INT(ArchiveEntry_GetMode(Archive_GetEntry(archive, 1)), 040755);
    CHECK_EQ_INT(ArchiveEntry_GetSize(Archive_GetEntry(archive, 1)), 0);
    CHECK_EQ_INT(ArchiveEntry_GetMode(Archive_GetEntry(archive, 2)), 0);
    M_CheckReads(
        archive, Archive_GetEntry(archive, 0), "hello", &test.allocator);
    M_CheckReads(
        archive, Archive_GetEntry(archive, 2), "archive", &test.allocator);
    M_CheckReads(archive, Archive_GetEntry(archive, 1), "", &test.allocator);
    Archive_Close(archive);
    CHECK_EQ_INT(test.live_blocks, 0);
    CHECK_EQ_INT(test.size_errors, 0);
}

TEST(path_lookup_matches_the_whole_path_exactly)
{
    const M_ENTRY entries[] = {
        { "dir/File.txt", "a", 0 },
        { "File.txt", "b", 0 },
    };
    const M_ARCHIVE built = M_Build(entries, 2);
    ARCHIVE *archive;
    CHECK(Result_Log(
        M_Open(&built, nullptr, &M_PathFormat, &g_HeapAllocator, &archive),
        LOG_LEVEL_ERROR));
    CHECK(
        Archive_Find(archive, "dir/File.txt") == Archive_GetEntry(archive, 0));
    CHECK(Archive_Find(archive, "File.txt") == Archive_GetEntry(archive, 1));
    CHECK(Archive_Find(archive, "dir/file.txt") == nullptr);
    CHECK(Archive_Find(archive, "other/File.txt") == nullptr);
    CHECK(Archive_Find(archive, "dir") == nullptr);
    Archive_Close(archive);
}

TEST(last_nocase_lookup_matches_the_last_component_in_either_case)
{
    const M_ENTRY entries[] = {
        { "Title00.JPG", "a", 0 },
    };
    const M_ARCHIVE built = M_Build(entries, 1);
    ARCHIVE *archive;
    CHECK(Result_Log(
        M_Open(
            &built, nullptr, &M_LastNocaseFormat, &g_HeapAllocator, &archive),
        LOG_LEVEL_ERROR));
    const ARCHIVE_ENTRY *const entry = Archive_GetEntry(archive, 0);
    CHECK(Archive_Find(archive, "Title00.JPG") == entry);
    CHECK(Archive_Find(archive, "title00.jpg") == entry);
    CHECK(Archive_Find(archive, "data/title00.jpg") == entry);
    CHECK(Archive_Find(archive, "a/b/TITLE00.jpg") == entry);
    CHECK(Archive_Find(archive, "title00.jpg/") == nullptr);
    CHECK(Archive_Find(archive, "title00") == nullptr);
    Archive_Close(archive);
}

TEST(the_later_of_two_entries_with_one_key_is_found)
{
    const M_ENTRY entries[] = {
        { "a.txt", "first", 0 },
        { "A.TXT", "second", 0 },
    };
    const M_ARCHIVE built = M_Build(entries, 2);
    ARCHIVE *archive;
    CHECK(Result_Log(
        M_Open(
            &built, nullptr, &M_LastNocaseFormat, &g_HeapAllocator, &archive),
        LOG_LEVEL_ERROR));
    CHECK_EQ_INT(Archive_GetCount(archive), 2);
    CHECK(Archive_Find(archive, "a.txt") == Archive_GetEntry(archive, 1));
    Archive_Close(archive);
}

TEST(a_failed_check_names_the_archive_and_the_entry)
{
    const M_ENTRY entries[] = {
        { "a.txt", "abc", 0 },
        { "b.txt", "def", 0 },
    };
    M_ARCHIVE built = M_Build(entries, 2);
    built.bytes[built.size - 4] ^= 1;
    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    ARCHIVE *archive;
    CHECK(Result_Log(
        M_Open(&built, "test.arc", &M_PathFormat, &test.allocator, &archive),
        LOG_LEVEL_ERROR));

    uint8_t *data = (uint8_t *)"";
    const RESULT result =
        Archive_Read(archive, Archive_Find(archive, "a.txt"), &data);
    CHECK(!IS_OK(result));
    CHECK(data == nullptr);
    CHECK(Str_StartsWith(result.message, "test.arc: a.txt: check value"));
    Result_Ignore(result);
    M_CheckReads(
        archive, Archive_Find(archive, "b.txt"), "def", &test.allocator);
    Archive_Close(archive);
    CHECK_EQ_INT(test.live_blocks, 0);
}

TEST(a_failed_read_of_an_unnamed_archive_names_the_entry)
{
    const M_ENTRY entries[] = {
        { "a.txt", "abc", 0 },
        { "b.txt", "def", 0 },
    };
    M_ARCHIVE built = M_Build(entries, 2);
    built.size -= 1;
    ARCHIVE *archive;
    CHECK(Result_Log(
        M_Open(&built, nullptr, &M_PathFormat, &g_HeapAllocator, &archive),
        LOG_LEVEL_ERROR));
    uint8_t *data;
    const RESULT result =
        Archive_Read(archive, Archive_GetEntry(archive, 1), &data);
    CHECK(!IS_OK(result));
    CHECK(Str_StartsWith(result.message, "b.txt: read of 3 bytes"));
    Result_Ignore(result);
    M_CheckReads(
        archive, Archive_GetEntry(archive, 0), "abc", &g_HeapAllocator);
    Archive_Close(archive);
}

TEST(a_failed_table_names_the_archive_and_closes_the_reader)
{
    const M_ENTRY entries[] = {
        { "a.txt", "abc", 0 },
    };
    const M_ARCHIVE built = M_Build(entries, 1);
    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    const size_t size = 4 + 1 + strlen("a.txt") + 6;
    void *const bytes = Allocator_Alloc(&test.allocator, size);
    memcpy(bytes, built.bytes, size);
    READER reader;
    Reader_OpenOwnedBuffer(&reader, bytes, size, &test.allocator, "test.arc");
    ARCHIVE *archive;
    const RESULT result =
        Archive_Open(&M_PathFormat, &reader, &test.allocator, &archive);
    CHECK(!IS_OK(result));
    CHECK(archive == nullptr);
    CHECK_EQ_STR(result.message, "test.arc: table runs past the end");
    Result_Ignore(result);
    CHECK_EQ_INT(test.live_blocks, 0);
}

TEST(an_unnamed_archive_fails_without_a_name)
{
    const uint8_t bytes[] = { 9, 0, 0, 0 };
    READER reader;
    Reader_OpenBuffer(&reader, bytes, sizeof(bytes), nullptr);
    ARCHIVE *archive;
    const RESULT result =
        Archive_Open(&M_PathFormat, &reader, &g_HeapAllocator, &archive);
    CHECK(!IS_OK(result));
    CHECK_EQ_STR(result.message, "too many entries");
    Result_Ignore(result);
}

static VFS *M_Mount(
    const M_ARCHIVE *const built, const char *const name,
    const ARCHIVE_FORMAT *const format, const ALLOCATOR *const allocator)
{
    ARCHIVE *archive;
    CHECK(Result_Log(
        M_Open(built, name, format, allocator, &archive), LOG_LEVEL_ERROR));
    VFS *const vfs = Vfs_Create(allocator);
    Vfs_MountSource(vfs, Archive_ToSource(archive), 0);
    return vfs;
}

TEST(a_source_opens_entries_in_readers_named_by_the_archive)
{
    const M_ENTRY entries[] = {
        { "Title00.JPG", "image", 0 },
    };
    const M_ARCHIVE built = M_Build(entries, 1);
    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    VFS *const vfs =
        M_Mount(&built, "test.arc", &M_LastNocaseFormat, &test.allocator);

    CHECK(Vfs_Exists(vfs, "data/title00.jpg"));
    CHECK(Vfs_Exists(vfs, "DATA\\TITLE00.JPG"));
    CHECK(!Vfs_Exists(vfs, "data/title01.jpg"));
    READER reader;
    CHECK(Result_Log(
        Vfs_Open(vfs, "data/title00.jpg", VFS_STREAM, &reader),
        LOG_LEVEL_ERROR));
    CHECK_EQ_STR(Reader_GetName(&reader), "test.arc/Title00.JPG");
    CHECK(M_ReadsAs(&reader, "image"));
    Reader_Close(&reader);
    Vfs_Destroy(vfs);
    CHECK_EQ_INT(test.live_blocks, 0);
    CHECK_EQ_INT(test.size_errors, 0);
}

TEST(a_source_of_an_unnamed_archive_names_readers_by_the_entry)
{
    const M_ENTRY entries[] = {
        { "a.txt", "text", 0 },
    };
    const M_ARCHIVE built = M_Build(entries, 1);
    ARCHIVE *archive;
    CHECK(Result_Log(
        M_Open(&built, nullptr, &M_PathFormat, &g_HeapAllocator, &archive),
        LOG_LEVEL_ERROR));
    VFS_SOURCE *const source = Archive_ToSource(archive);
    CHECK_EQ_STR(source->name, "archive");
    READER reader;
    CHECK(Result_Log(
        source->funcs->open(source, "a.txt", VFS_IN_MEMORY, &reader),
        LOG_LEVEL_ERROR));
    CHECK_EQ_STR(Reader_GetName(&reader), "a.txt");
    CHECK(M_ReadsAs(&reader, "text"));
    Reader_Close(&reader);
    source->funcs->close(source);
}

TEST(a_path_source_finds_whole_paths_in_either_case_but_no_directories)
{
    const M_ENTRY entries[] = {
        { "Dir/", "", 040755 },
        { "Dir/File.txt", "a", 0 },
    };
    const M_ARCHIVE built = M_Build(entries, 2);
    VFS *const vfs =
        M_Mount(&built, "test.arc", &M_PathFormat, &g_HeapAllocator);
    CHECK(Vfs_Exists(vfs, "dir/file.txt"));
    CHECK(Vfs_Exists(vfs, "DIR\\File.txt"));
    CHECK(!Vfs_Exists(vfs, "file.txt"));
    CHECK(!Vfs_Exists(vfs, "dir"));
    CHECK(!Vfs_Exists(vfs, "dir/"));
    Vfs_Destroy(vfs);
}

typedef struct {
    char *paths[M_MAX_ENTRIES];
    int32_t count;
} M_VISITED;

static void M_Visit(void *const user, const char *const path)
{
    M_VISITED *const visited = user;
    if (visited->count < M_MAX_ENTRIES) {
        visited->paths[visited->count] = Str_Dup(path);
    }
    visited->count++;
}

TEST(a_source_lists_files_folded_in_table_order)
{
    const M_ENTRY entries[] = {
        { "B.txt", "b", 0 },
        { "Dir/", "", 040755 },
        { "Dir/A.TXT", "a", 0 },
    };
    const M_ARCHIVE built = M_Build(entries, 3);
    ARCHIVE *archive;
    CHECK(Result_Log(
        M_Open(&built, "test.arc", &M_PathFormat, &g_HeapAllocator, &archive),
        LOG_LEVEL_ERROR));
    VFS_SOURCE *const source = Archive_ToSource(archive);
    M_VISITED visited = { .count = 0 };
    source->funcs->list(source, M_Visit, &visited);
    CHECK_EQ_INT(visited.count, 2);
    CHECK_EQ_STR(visited.paths[0], "b.txt");
    CHECK_EQ_STR(visited.paths[1], "dir/a.txt");
    for (int32_t i = 0; i < visited.count && i < M_MAX_ENTRIES; i++) {
        Mem_Free(visited.paths[i]);
    }
    source->funcs->close(source);
}

TEST(a_source_fails_a_missing_entry_and_a_failed_read)
{
    const M_ENTRY entries[] = {
        { "a.txt", "abc", 0 },
    };
    M_ARCHIVE built = M_Build(entries, 1);
    built.bytes[built.size - 1] ^= 1;
    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    ARCHIVE *archive;
    CHECK(Result_Log(
        M_Open(&built, "test.arc", &M_PathFormat, &test.allocator, &archive),
        LOG_LEVEL_ERROR));
    VFS_SOURCE *const source = Archive_ToSource(archive);
    READER reader;
    RESULT result = source->funcs->open(source, "b.txt", VFS_STREAM, &reader);
    CHECK_EQ_STR(result.message, "test.arc: b.txt: not found");
    Result_Ignore(result);
    result = source->funcs->open(source, "a.txt", VFS_STREAM, &reader);
    CHECK(!IS_OK(result));
    CHECK(Str_StartsWith(result.message, "test.arc: a.txt: check value"));
    Result_Ignore(result);
    source->funcs->close(source);
    CHECK_EQ_INT(test.live_blocks, 0);
}
