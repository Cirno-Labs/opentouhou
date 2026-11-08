#include "core/vfs.h"

#include "test.h"
#include "test_allocator.h"

#include <string.h>

// A source that answers to one fixed path with bytes given at creation,
// used both as the mounted "file" that Vfs_MountFile reads from and as the
// source an opener builds from it.
typedef struct {
    VFS_SOURCE base;
    const char *path;
    char *data;
    size_t size;
} FAKE_SOURCE;

static bool M_Contains(const VFS_SOURCE *const source, const char *const path)
{
    const FAKE_SOURCE *const fake = (const FAKE_SOURCE *)source;
    return strcmp(path, fake->path) == 0;
}

static RESULT M_Open(
    VFS_SOURCE *const source, [[maybe_unused]] const char *const path,
    [[maybe_unused]] const VFS_MODE mode, READER *const out)
{
    const FAKE_SOURCE *const fake = (const FAKE_SOURCE *)source;
    Reader_OpenBuffer(out, fake->data, fake->size);
    return OK;
}

static void M_Close(VFS_SOURCE *const source)
{
    FAKE_SOURCE *const fake = (FAKE_SOURCE *)source;
    Mem_Free(fake->data);
    Mem_Free(fake);
}

static const VFS_SOURCE_FUNCS M_Funcs = {
    .contains = M_Contains,
    .open = M_Open,
    .close = M_Close,
};

static VFS_SOURCE *M_CreateFakeFile(
    const char *const path, const void *const data, const size_t size)
{
    FAKE_SOURCE *const fake = Mem_AllocZ(sizeof(*fake));
    fake->base.funcs = &M_Funcs;
    fake->base.name = path;
    fake->path = path;
    fake->data = Mem_Dup(data, size);
    fake->size = size;
    return &fake->base;
}

static bool m_OpenerSawAllocator;

static RESULT M_Opener(
    READER *const file, [[maybe_unused]] const char *const name,
    const ALLOCATOR *const allocator, VFS_SOURCE **const out)
{
    m_OpenerSawAllocator = allocator != nullptr;
    const size_t size = Reader_Size(file);
    void *const data = Reader_ReadAlloc(file, size, allocator);
    const RESULT result = Reader_Result(file);
    Reader_Close(file);
    if (!IS_OK(result)) {
        return result;
    }
    *out = M_CreateFakeFile("built", data, size);
    Allocator_Free(allocator, data, size);
    return OK;
}

static RESULT M_RefusingOpener(
    READER *const file, const char *const name,
    [[maybe_unused]] const ALLOCATOR *const allocator,
    [[maybe_unused]] VFS_SOURCE **const out)
{
    Reader_Close(file);
    return FAIL("%s: refused", name);
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

TEST(the_opener_builds_a_source_from_the_file_and_it_is_mounted)
{
    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    VFS *const vfs = Vfs_Create(&test.allocator);
    Vfs_MountSource(vfs, M_CreateFakeFile("archive.dat", "hello", 5), 0);

    m_OpenerSawAllocator = false;
    CHECK(Result_Log(
        Vfs_MountFile(vfs, "archive.dat", M_Opener, 100), LOG_LEVEL_ERROR));
    CHECK(m_OpenerSawAllocator);

    READER reader;
    CHECK(Result_Log(
        Vfs_Open(vfs, "built", VFS_IN_MEMORY, &reader), LOG_LEVEL_ERROR));
    CHECK(M_ReadsAs(&reader, "hello"));
    Reader_Close(&reader);
    Vfs_Destroy(vfs);
    CHECK_EQ_INT(test.live_blocks, 0);
}

TEST(a_missing_path_fails_without_calling_the_opener)
{
    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    VFS *const vfs = Vfs_Create(&test.allocator);

    const RESULT result =
        Vfs_MountFile(vfs, "missing.dat", M_RefusingOpener, 0);
    CHECK(!IS_OK(result));
    Result_Ignore(result);
    Vfs_Destroy(vfs);
    CHECK_EQ_INT(test.live_blocks, 0);
}

TEST(an_opener_that_refuses_closes_the_reader_and_fails_the_mount)
{
    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    VFS *const vfs = Vfs_Create(&test.allocator);
    Vfs_MountSource(vfs, M_CreateFakeFile("archive.dat", "hello", 5), 0);

    const RESULT result =
        Vfs_MountFile(vfs, "archive.dat", M_RefusingOpener, 0);
    CHECK(!IS_OK(result));
    CHECK(strstr(result.message, "archive.dat") != nullptr);
    Result_Ignore(result);
    Vfs_Destroy(vfs);
    CHECK_EQ_INT(test.live_blocks, 0);
}
