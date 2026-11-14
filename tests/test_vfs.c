#include "core/vfs.h"

#include "test.h"
#include "test_allocator.h"

#include <string.h>

typedef struct {
    const char *path;
    const char *data;
    size_t size;
    bool can_stream;
} FAKE_ENTRY;

typedef struct {
    VFS_SOURCE base;
    const FAKE_ENTRY *entries;
    int32_t count;
    const ALLOCATOR *allocator;
    int32_t *close_count;
} FAKE_SOURCE;

static const FAKE_ENTRY *M_FindEntry(
    const FAKE_SOURCE *const fake, const char *const path)
{
    for (int32_t i = 0; i < fake->count; i++) {
        if (strcmp(fake->entries[i].path, path) == 0) {
            return &fake->entries[i];
        }
    }
    return nullptr;
}

static bool M_Contains(const VFS_SOURCE *const source, const char *const path)
{
    return M_FindEntry((const FAKE_SOURCE *)source, path) != nullptr;
}

static RESULT M_Open(
    VFS_SOURCE *const source, const char *const path, const VFS_MODE mode,
    READER *const out)
{
    FAKE_SOURCE *const fake = (FAKE_SOURCE *)source;
    const FAKE_ENTRY *const entry = M_FindEntry(fake, path);
    FAIL_IF(entry == nullptr, "%s: not found in %s", path, source->name);
    if (mode == VFS_STREAM && entry->can_stream) {
        Reader_OpenBuffer(out, entry->data, entry->size, nullptr);
        return OK;
    }
    void *const copy = Allocator_Alloc(fake->allocator, entry->size);
    memcpy(copy, entry->data, entry->size);
    Reader_OpenOwnedBuffer(out, copy, entry->size, fake->allocator, nullptr);
    return OK;
}

static void M_List(
    const VFS_SOURCE *const source,
    void (*const visit)(void *user, const char *path), void *const user)
{
    const FAKE_SOURCE *const fake = (const FAKE_SOURCE *)source;
    for (int32_t i = 0; i < fake->count; i++) {
        visit(user, fake->entries[i].path);
    }
}

static void M_Close(VFS_SOURCE *const source)
{
    FAKE_SOURCE *const fake = (FAKE_SOURCE *)source;
    if (fake->close_count != nullptr) {
        (*fake->close_count)++;
    }
    Allocator_Free(fake->allocator, fake, sizeof(*fake));
}

static const VFS_SOURCE_FUNCS M_Funcs = {
    .contains = M_Contains,
    .open = M_Open,
    .list = M_List,
    .close = M_Close,
};

static VFS_SOURCE *M_CreateFake(
    const ALLOCATOR *const allocator, const char *const name,
    const FAKE_ENTRY *const entries, const int32_t count,
    int32_t *const close_count)
{
    FAKE_SOURCE *const fake = Allocator_AllocZ(allocator, sizeof(*fake));
    fake->base.funcs = &M_Funcs;
    fake->base.name = name;
    fake->entries = entries;
    fake->count = count;
    fake->allocator = allocator;
    fake->close_count = close_count;
    return &fake->base;
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

TEST(a_higher_priority_source_wins)
{
    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    VFS *const vfs = Vfs_Create(&test.allocator);
    const FAKE_ENTRY low[] = { { "data/a.txt", "low", 3, true } };
    const FAKE_ENTRY high[] = { { "data/a.txt", "high", 4, true } };
    Vfs_MountSource(
        vfs, M_CreateFake(&test.allocator, "low", low, 1, nullptr), 0);
    Vfs_MountSource(
        vfs, M_CreateFake(&test.allocator, "high", high, 1, nullptr), 10);

    READER reader;
    CHECK(Result_Log(
        Vfs_Open(vfs, "data/a.txt", VFS_IN_MEMORY, &reader), LOG_LEVEL_ERROR));
    CHECK(M_ReadsAs(&reader, "high"));
    Reader_Close(&reader);
    Vfs_Destroy(vfs);
    CHECK_EQ_INT(test.live_blocks, 0);
}

TEST(a_later_mount_wins_an_equal_priority)
{
    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    VFS *const vfs = Vfs_Create(&test.allocator);
    const FAKE_ENTRY first[] = { { "data/a.txt", "first", 5, true } };
    const FAKE_ENTRY second[] = { { "data/a.txt", "second", 6, true } };
    Vfs_MountSource(
        vfs, M_CreateFake(&test.allocator, "first", first, 1, nullptr), 5);
    Vfs_MountSource(
        vfs, M_CreateFake(&test.allocator, "second", second, 1, nullptr), 5);

    READER reader;
    CHECK(Result_Log(
        Vfs_Open(vfs, "data/a.txt", VFS_IN_MEMORY, &reader), LOG_LEVEL_ERROR));
    CHECK(M_ReadsAs(&reader, "second"));
    Reader_Close(&reader);
    Vfs_Destroy(vfs);
}

TEST(a_stream_open_never_fails_when_a_source_cannot_stream)
{
    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    VFS *const vfs = Vfs_Create(&test.allocator);
    const FAKE_ENTRY entries[] = { { "archive/x.dat", "packed", 6, false } };
    Vfs_MountSource(
        vfs, M_CreateFake(&test.allocator, "archive", entries, 1, nullptr), 0);

    READER reader;
    CHECK(Result_Log(
        Vfs_Open(vfs, "archive/x.dat", VFS_STREAM, &reader), LOG_LEVEL_ERROR));
    CHECK(M_ReadsAs(&reader, "packed"));
    Reader_Close(&reader);
    Vfs_Destroy(vfs);
    CHECK_EQ_INT(test.live_blocks, 0);
}

TEST(destroy_closes_every_source_exactly_once)
{
    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    VFS *const vfs = Vfs_Create(&test.allocator);
    const FAKE_ENTRY a[] = { { "a.txt", "a", 1, true } };
    const FAKE_ENTRY b[] = { { "b.txt", "b", 1, true } };
    int32_t closed_a = 0;
    int32_t closed_b = 0;
    Vfs_MountSource(
        vfs, M_CreateFake(&test.allocator, "a", a, 1, &closed_a), 0);
    Vfs_MountSource(
        vfs, M_CreateFake(&test.allocator, "b", b, 1, &closed_b), 1);
    Vfs_Destroy(vfs);
    CHECK_EQ_INT(closed_a, 1);
    CHECK_EQ_INT(closed_b, 1);
    CHECK_EQ_INT(test.live_blocks, 0);
}

static RESULT M_UppercaseHook(
    void *const context, const char *const path, READER *const file)
{
    int32_t *const calls = context;
    (*calls)++;
    const size_t size = Reader_Size(file);
    char *const upper = Mem_Alloc(size);
    Reader_ReadBytes(file, upper, size);
    for (size_t i = 0; i < size; i++) {
        if (upper[i] >= 'a' && upper[i] <= 'z') {
            upper[i] = (char)(upper[i] - 'a' + 'A');
        }
    }
    FAIL_IF(!IS_OK(Reader_Result(file)), "%s: cannot read for the hook", path);
    Reader_Close(file);
    Reader_OpenOwnedBuffer(file, upper, size, &g_HeapAllocator, nullptr);
    return OK;
}

TEST(the_hook_runs_for_in_memory_opens_and_can_replace_content)
{
    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    VFS *const vfs = Vfs_Create(&test.allocator);
    const FAKE_ENTRY entries[] = { { "a.txt", "hello", 5, true } };
    Vfs_MountSource(
        vfs, M_CreateFake(&test.allocator, "a", entries, 1, nullptr), 0);
    int32_t calls = 0;
    Vfs_SetHook(vfs, M_UppercaseHook, &calls);

    READER reader;
    CHECK(Result_Log(
        Vfs_Open(vfs, "a.txt", VFS_IN_MEMORY, &reader), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(calls, 1);
    CHECK(M_ReadsAs(&reader, "HELLO"));
    Reader_Close(&reader);
    Vfs_Destroy(vfs);
}

TEST(the_hook_does_not_run_for_stream_opens)
{
    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    VFS *const vfs = Vfs_Create(&test.allocator);
    const FAKE_ENTRY entries[] = { { "a.txt", "hello", 5, true } };
    Vfs_MountSource(
        vfs, M_CreateFake(&test.allocator, "a", entries, 1, nullptr), 0);
    int32_t calls = 0;
    Vfs_SetHook(vfs, M_UppercaseHook, &calls);

    READER reader;
    CHECK(Result_Log(
        Vfs_Open(vfs, "a.txt", VFS_STREAM, &reader), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(calls, 0);
    CHECK(M_ReadsAs(&reader, "hello"));
    Reader_Close(&reader);
    Vfs_Destroy(vfs);
}

static RESULT M_FailingHook(
    [[maybe_unused]] void *const context, const char *const path,
    [[maybe_unused]] READER *const file)
{
    return FAIL("%s: the hook refuses", path);
}

TEST(a_failing_hook_fails_the_open_and_closes_the_reader)
{
    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    VFS *const vfs = Vfs_Create(&test.allocator);
    const FAKE_ENTRY entries[] = { { "a.txt", "hello", 5, true } };
    Vfs_MountSource(
        vfs, M_CreateFake(&test.allocator, "a", entries, 1, nullptr), 0);
    Vfs_SetHook(vfs, M_FailingHook, nullptr);

    READER reader;
    const RESULT result = Vfs_Open(vfs, "a.txt", VFS_IN_MEMORY, &reader);
    CHECK(!IS_OK(result));
    Result_Ignore(result);
    Vfs_Destroy(vfs);
    CHECK_EQ_INT(test.live_blocks, 0);
}

TEST(paths_normalise_case_and_separators)
{
    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    VFS *const vfs = Vfs_Create(&test.allocator);
    const FAKE_ENTRY entries[] = { { "bgm/th06_01.wav", "x", 1, true } };
    Vfs_MountSource(
        vfs, M_CreateFake(&test.allocator, "a", entries, 1, nullptr), 0);

    CHECK(Vfs_Exists(vfs, "bgm/th06_01.wav"));
    CHECK(Vfs_Exists(vfs, "BGM\\TH06_01.WAV"));
    CHECK(Vfs_Exists(vfs, "//bgm///th06_01.wav"));
    CHECK(!Vfs_Exists(vfs, "bgm/missing.wav"));
    Vfs_Destroy(vfs);
}

TEST(a_double_dot_component_is_rejected)
{
    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    VFS *const vfs = Vfs_Create(&test.allocator);

    CHECK(!Vfs_Exists(vfs, "data/../secret.txt"));
    READER reader;
    const RESULT result =
        Vfs_Open(vfs, "data/../secret.txt", VFS_STREAM, &reader);
    CHECK(!IS_OK(result));
    Result_Ignore(result);
    Vfs_Destroy(vfs);
}

TEST(an_absolute_path_is_rejected)
{
    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    VFS *const vfs = Vfs_Create(&test.allocator);

    CHECK(!Vfs_Exists(vfs, "C:/windows/system32"));
    Vfs_Destroy(vfs);
}

TEST(a_missing_file_names_the_path_and_the_sources)
{
    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    VFS *const vfs = Vfs_Create(&test.allocator);
    const FAKE_ENTRY entries[] = { { "a.txt", "x", 1, true } };
    Vfs_MountSource(
        vfs, M_CreateFake(&test.allocator, "紅魔郷CM.DAT", entries, 1, nullptr),
        0);

    READER reader;
    const RESULT result = Vfs_Open(vfs, "Missing.TXT", VFS_IN_MEMORY, &reader);
    CHECK(!IS_OK(result));
    CHECK(strstr(result.message, "Missing.TXT") != nullptr);
    CHECK(strstr(result.message, "missing.txt") != nullptr);
    CHECK(strstr(result.message, "紅魔郷CM.DAT") != nullptr);
    Result_Ignore(result);
    Vfs_Destroy(vfs);
}
