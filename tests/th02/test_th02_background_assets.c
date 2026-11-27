#include "tests/test.h"
#include "tests/test_allocator.h"
#include "th02/background/assets.h"

#include "core/base/memory.h"
#include "core/io/reader.h"

#include <string.h>

typedef struct {
    char path[11];
    uint8_t *bytes;
    size_t size;
} FILE_ENTRY;

typedef struct {
    VFS_SOURCE base;
    FILE_ENTRY files[2];
    TEST_ALLOCATOR *readers;
} SOURCE;

static bool M_Contains(const VFS_SOURCE *const base, const char *const path)
{
    const SOURCE *const source = (const SOURCE *)base;
    for (size_t i = 0; i < ARRAY_SIZE(source->files); i++) {
        if (strcmp(source->files[i].path, path) == 0) {
            return true;
        }
    }
    return false;
}

static RESULT M_Open(
    VFS_SOURCE *const base, const char *const path, const VFS_MODE mode,
    READER *const out)
{
    SOURCE *const source = (SOURCE *)base;
    for (size_t i = 0; i < ARRAY_SIZE(source->files); i++) {
        const FILE_ENTRY *const file = &source->files[i];
        if (strcmp(file->path, path) != 0) {
            continue;
        }
        uint8_t *const bytes =
            Allocator_Alloc(&source->readers->allocator, file->size);
        memcpy(bytes, file->bytes, file->size);
        Reader_OpenOwnedBuffer(
            out, bytes, file->size, &source->readers->allocator, nullptr);
        return OK;
    }
    return FAIL("Unknown synthetic stage resource");
}

static void M_Close(VFS_SOURCE *const base)
{
    SOURCE *const source = (SOURCE *)base;
    for (size_t i = 0; i < ARRAY_SIZE(source->files); i++) {
        Mem_Free(source->files[i].bytes);
    }
    Mem_Free(source);
}

static const VFS_SOURCE_FUNCS m_Funcs = {
    .contains = M_Contains,
    .open = M_Open,
    .close = M_Close,
};

static SOURCE *M_Source(TEST_ALLOCATOR *const readers, const uint8_t stage)
{
    SOURCE *const source = Mem_AllocZ(sizeof(*source));
    source->base.funcs = &m_Funcs;
    source->base.name = "synthetic background assets";
    source->readers = readers;
    static const char *const names[] = { "stage0.map", "stage0.mpn" };
    static const size_t sizes[] = { 3082, 310 };
    for (size_t i = 0; i < 2; i++) {
        FILE_ENTRY *const file = &source->files[i];
        memcpy(file->path, names[i], 11);
        file->path[5] += stage;
        file->size = sizes[i];
        file->bytes = Mem_AllocZ(file->size);
    }
    memcpy(source->files[0].bytes, "MIKO_MAP  ", 10);
    memset(source->files[0].bytes + 10, 1, 3072);
    uint8_t *const mpn = source->files[1].bytes;
    memcpy(mpn, "MPTN", 4);
    mpn[4] = 1;
    memset(mpn + 6, 0x30, 48);
    memset(mpn + 54, 0xff, 32);
    memset(mpn + 54 + 128, 0xff, 32);
    return source;
}

TEST(loads_map_and_tiles_and_frees_them_after_the_vfs_closes)
{
    TEST_ALLOCATOR readers;
    TEST_ALLOCATOR data;
    TestAllocator_Init(&readers);
    TestAllocator_Init(&data);
    VFS *const files = Vfs_Create(&g_HeapAllocator);
    SOURCE *const source = M_Source(&readers, 2);
    Vfs_MountSource(files, "", &source->base, 0);
    BACKGROUND_ASSETS assets;
    CHECK(Result_Log(
        BackgroundAssets_Load(files, 2, &data.allocator, &assets),
        LOG_LEVEL_ERROR));
    CHECK_EQ_INT(readers.live_blocks, 0);
    Vfs_Destroy(files);
    CHECK_EQ_INT(assets.map.sections[0].tiles[0][0], 1);
    CHECK_EQ_INT(assets.tiles.count, 2);
    BackgroundAssets_Free(&assets);
    CHECK_EQ_INT(data.live_blocks, 0);
    const uint8_t *const bytes = (const uint8_t *)&assets;
    for (size_t i = 0; i < sizeof(assets); i++) {
        CHECK_EQ_INT(bytes[i], 0);
    }
}

TEST(missing_map_fails_and_leaves_assets_zeroed)
{
    TEST_ALLOCATOR readers;
    TEST_ALLOCATOR data;
    TestAllocator_Init(&readers);
    TestAllocator_Init(&data);
    VFS *const files = Vfs_Create(&g_HeapAllocator);
    BACKGROUND_ASSETS assets;
    memset(&assets, 0xa5, sizeof(assets));
    CHECK(!Result_Ignore(
        BackgroundAssets_Load(files, 0, &data.allocator, &assets)));
    Vfs_Destroy(files);
    const uint8_t *const bytes = (const uint8_t *)&assets;
    for (size_t i = 0; i < sizeof(assets); i++) {
        CHECK_EQ_INT(bytes[i], 0);
    }
}

TEST(stage_above_five_is_rejected)
{
    TEST_ALLOCATOR data;
    TestAllocator_Init(&data);
    VFS *const files = Vfs_Create(&g_HeapAllocator);
    BACKGROUND_ASSETS assets;
    CHECK(!Result_Ignore(
        BackgroundAssets_Load(files, 6, &data.allocator, &assets)));
    Vfs_Destroy(files);
}
