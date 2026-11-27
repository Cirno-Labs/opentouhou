#include "tests/test.h"
#include "tests/test_allocator.h"
#include "th02/stage/assets.h"

typedef struct {
    char path[11];
    uint8_t *bytes;
    size_t size;
} FILE_ENTRY;

typedef struct {
    VFS_SOURCE base;
    FILE_ENTRY files[6];
    TEST_ALLOCATOR *readers;
    int32_t fail_index;
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
        FAIL_IF((int32_t)i == source->fail_index, "Synthetic open failure");
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

static void M_U16(uint8_t *const bytes, const uint16_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
}

static SOURCE *M_Source(TEST_ALLOCATOR *const readers, const uint8_t stage)
{
    SOURCE *const source = Mem_AllocZ(sizeof(*source));
    source->base.funcs = &m_Funcs;
    source->base.name = "synthetic stage";
    source->readers = readers;
    source->fail_index = -1;
    static const char *const names[] = {
        "stage0.bft", "stage0.bmt", "stage0.bbt",
        "stage0.map", "stage0.mpn", "stage0.dt1",
    };
    for (size_t i = 0; i < 6; i++) {
        FILE_ENTRY *const file = &source->files[i];
        memcpy(file->path, names[i], 11);
        file->path[5] += stage;
        file->size = i < 3 ? 88 : i == 3 ? 3082 : i == 4 ? 310 : 143;
        file->bytes = Mem_AllocZ(file->size);
        if (i < 3) {
            memcpy(file->bytes, "BFNT\x1a", 5);
            file->bytes[5] = i == 1 ? 3 : 0x83;
            M_U16(file->bytes + 8, 8);
            M_U16(file->bytes + 10, 2);
            M_U16(file->bytes + 12, 10);
            M_U16(file->bytes + 14, 10);
            if (i != 1) {
                memset(file->bytes + 32, (int)(i + 2) * 16, 48);
            }
            memset(file->bytes + (i == 1 ? 32 : 80), (int)(i + 1) * 17, 8);
        }
    }
    memset(source->files[3].bytes + 10, 1, 3072);
    uint8_t *const mpn = source->files[4].bytes;
    memcpy(mpn, "MPTN", 4);
    mpn[4] = 1;
    memset(mpn + 6, 0x30, 48);
    memset(mpn + 54, 0xff, 32);
    memset(mpn + 54 + 128, 0xff, 32);
    uint8_t *const dt1 = source->files[5].bytes;
    M_U16(dt1, 139);
    M_U16(dt1 + 4, 3);
    M_U16(dt1 + 9, 1);
    M_U16(dt1 + 19, 128);
    M_U16(dt1 + 21, 1);
    M_U16(dt1 + 23, 1);
    M_U16(dt1 + 33, 2);
    M_U16(dt1 + 37, 1);
    dt1[39] = 0x80;
    dt1[40] = 0xff;
    M_U16(dt1 + 41, 3);
    M_U16(dt1 + 43, 7);
    M_U16(dt1 + 93, 1);
    memset(dt1 + 45, 0xff, 48);
    memset(dt1 + 95, 0xff, 48);
    dt1[45] = 0;
    return source;
}

static void M_Pattern(PATTERN_BANK *const bank, const uint16_t slot)
{
    uint8_t pixels[16];
    memset(pixels, 7, sizeof(pixels));
    CHECK(Result_Ignore(PatternBank_Put(bank, slot, 8, 2, pixels, 0)));
}

static void M_Startup(PATTERN_BANK *const bank, TEST_ALLOCATOR *const allocator)
{
    PatternBank_Init(bank, &allocator->allocator);
    for (uint16_t slot = 0; slot < 128; slot++) {
        M_Pattern(bank, slot);
    }
}

static void M_Empty(const STAGE_ASSETS *const assets)
{
    const uint8_t *const bytes = (const uint8_t *)assets;
    for (size_t i = 0; i < sizeof(*assets); i++) {
        CHECK_EQ_INT(bytes[i], 0);
    }
}

TEST(all_stages_keep_owned_data_after_namespace_close_and_replace_patterns)
{
    TEST_ALLOCATOR readers;
    TEST_ALLOCATOR data;
    TEST_ALLOCATOR patterns;
    TestAllocator_Init(&readers);
    TestAllocator_Init(&data);
    TestAllocator_Init(&patterns);
    PATTERN_BANK bank;
    M_Startup(&bank, &patterns);
    const PATTERN common = bank.entries[127];
    for (uint8_t stage = 0; stage < 6; stage++) {
        M_Pattern(&bank, 192);
        VFS *const files = Vfs_Create(&g_HeapAllocator);
        SOURCE *const source = M_Source(&readers, stage);
        Vfs_MountSource(files, "", &source->base, 0);
        STAGE_ASSETS assets;
        CHECK(Result_Log(
            StageAssets_Load(files, stage, &data.allocator, &bank, &assets),
            LOG_LEVEL_ERROR));
        CHECK_EQ_INT(readers.live_blocks, 0);
        Vfs_Destroy(files);
        CHECK_EQ_INT(assets.stage, stage);
        CHECK_EQ_INT(assets.pattern_first, 128);
        CHECK_EQ_INT(assets.pattern_count, 3);
        CHECK_EQ_INT(bank.next, 131);
        CHECK_EQ_INT(patterns.live_blocks, 131);
        CHECK(memcmp(&common, &bank.entries[127], sizeof(common)) == 0);
        for (uint16_t slot = 128; slot < 131; slot++) {
            CHECK_EQ_INT(bank.entries[slot].size, 0x0102);
            CHECK_EQ_INT(bank.entries[slot].data[0], 0xff);
            CHECK_EQ_INT(bank.entries[slot].data[2], slot == 129 ? 0 : 0xff);
        }
        CHECK_EQ_INT(bank.entries[192].size, 0);
        CHECK(assets.has_sprite_palette);
        CHECK_EQ_INT(assets.sprite_palette[1].red, 64);
        CHECK_EQ_INT(assets.background.map.sections[15].tiles[7][23], 1);
        CHECK_EQ_INT(assets.background.tiles.count, 2);
        CHECK_EQ_INT(assets.background.tiles.pixels[511], 1);
        CHECK_EQ_INT(assets.background.tiles.palette[0].red, 48);
        CHECK_EQ_INT(assets.data.map_count, 3);
        CHECK_EQ_INT(assets.data.formations[0].pattern, 128);
        CHECK_EQ_INT(assets.data.scripts[0].bytes[0], 0x80);
        CHECK_EQ_INT(assets.data.line_count, 2);
        CHECK_EQ_INT(assets.data.declared_line_count, 3);
        CHECK_EQ_INT(assets.data.lines[0].position, 7);
        StageAssets_Free(&assets);
        M_Empty(&assets);
        StageAssets_Free(&assets);
        CHECK_EQ_INT(patterns.live_blocks, 131);
        CHECK_EQ_INT(data.live_blocks, 0);
        CHECK_EQ_INT(data.live_bytes, 0);
    }
    PatternBank_Reset(&bank);
    CHECK_EQ_INT(patterns.live_blocks, 0);
    CHECK_EQ_INT(
        readers.size_errors + data.size_errors + patterns.size_errors, 0);
}

TEST(open_decode_and_capacity_failures_preserve_entire_existing_bank)
{
    TEST_ALLOCATOR readers;
    TEST_ALLOCATOR data;
    TEST_ALLOCATOR patterns;
    TestAllocator_Init(&readers);
    TestAllocator_Init(&data);
    TestAllocator_Init(&patterns);
    PATTERN_BANK bank;
    M_Startup(&bank, &patterns);
    M_Pattern(&bank, 130);
    PATTERN_BANK saved;
    for (int32_t failure = 0; failure < 16; failure++) {
        VFS *const files = Vfs_Create(&g_HeapAllocator);
        SOURCE *const source = M_Source(&readers, 3);
        Vfs_MountSource(files, "", &source->base, 0);
        if (failure < 6) {
            source->fail_index = failure;
        } else if (failure < 12) {
            source->files[failure - 6].size = 1;
        } else if (failure == 12) {
            source->files[1].bytes[5] = 0;
        } else if (failure == 13) {
            CHECK(Result_Ignore(PatternBank_SetNext(&bank, 511)));
        } else if (failure == 14) {
            CHECK(Result_Ignore(PatternBank_Cancel(&bank, 2)));
        }
        saved = bank;
        STAGE_ASSETS assets;
        CHECK(!Result_Ignore(StageAssets_Load(
            files, failure == 15 ? 6 : 3, &data.allocator, &bank, &assets)));
        M_Empty(&assets);
        CHECK(memcmp(&bank, &saved, sizeof(bank)) == 0);
        CHECK_EQ_INT(readers.live_blocks, 0);
        CHECK_EQ_INT(data.live_blocks, 0);
        CHECK_EQ_INT(data.live_bytes, 0);
        CHECK_EQ_INT(patterns.live_blocks, failure >= 14 ? 128 : 129);
        if (failure == 13) {
            CHECK(Result_Ignore(PatternBank_SetNext(&bank, 131)));
        }
        Vfs_Destroy(files);
    }
    PatternBank_Reset(&bank);
    CHECK_EQ_INT(patterns.live_blocks, 0);
    CHECK_EQ_INT(
        readers.size_errors + data.size_errors + patterns.size_errors, 0);
}

TEST(upper_slots_preserve_append_cursor_and_retained_slots_are_replaced)
{
    TEST_ALLOCATOR readers;
    TEST_ALLOCATOR data;
    TEST_ALLOCATOR patterns;
    TestAllocator_Init(&readers);
    TestAllocator_Init(&data);
    TestAllocator_Init(&patterns);
    PATTERN_BANK bank;
    M_Startup(&bank, &patterns);
    M_Pattern(&bank, 192);
    M_Pattern(&bank, 193);
    const PATTERN upper = bank.entries[193];
    M_Pattern(&bank, 194);
    CHECK(Result_Ignore(PatternBank_SetNext(&bank, 194)));
    VFS *const files = Vfs_Create(&g_HeapAllocator);
    SOURCE *const source = M_Source(&readers, 5);
    Vfs_MountSource(files, "", &source->base, 0);
    STAGE_ASSETS assets;
    CHECK(Result_Log(
        StageAssets_Load(files, 5, &data.allocator, &bank, &assets),
        LOG_LEVEL_ERROR));
    CHECK_EQ_INT(assets.pattern_first, 194);
    CHECK_EQ_INT(assets.pattern_count, 3);
    CHECK_EQ_INT(bank.next, 197);
    CHECK_EQ_INT(bank.entries[192].size, 0);
    CHECK(memcmp(&upper, &bank.entries[193], sizeof(upper)) == 0);
    CHECK_EQ_INT(patterns.live_blocks, 132);
    StageAssets_Free(&assets);
    Vfs_Destroy(files);
    PatternBank_Reset(&bank);
    CHECK_EQ_INT(
        patterns.live_blocks + data.live_blocks + readers.live_blocks, 0);
    CHECK_EQ_INT(
        readers.size_errors + data.size_errors + patterns.size_errors, 0);
}
