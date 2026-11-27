#include "th02_palette_fixture.h"
#include "th02_clip_fixture.h"
#include "th02/gameplay/assets.h"

#include "th02_sprites.h"
#include "test.h"
#include "test_allocator.h"

typedef struct {
    const char *path;
    uint8_t *bytes;
    size_t size;
} FILE_ENTRY;

typedef struct {
    VFS_SOURCE base;
    FILE_ENTRY files[5];
    TEST_ALLOCATOR *readers;
    int32_t fail_index;
} SOURCE;

typedef struct {
    uint8_t bytes[64];
    size_t size;
    size_t bits;
} PI_BUILDER;

static bool M_Contains(const VFS_SOURCE *const source, const char *const path)
{
    const SOURCE *const data = (const SOURCE *)source;
    for (size_t i = 0; i < ARRAY_SIZE(data->files); i++) {
        if (strcmp(data->files[i].path, path) == 0) {
            return true;
        }
    }
    return false;
}

static RESULT M_Open(
    VFS_SOURCE *const source, const char *const path, const VFS_MODE mode,
    READER *const out)
{
    SOURCE *const data = (SOURCE *)source;
    for (size_t i = 0; i < ARRAY_SIZE(data->files); i++) {
        if (strcmp(data->files[i].path, path) != 0) {
            continue;
        }
        FAIL_IF((int32_t)i == data->fail_index, "Synthetic resource failure");
        uint8_t *const bytes =
            Allocator_Alloc(&data->readers->allocator, data->files[i].size);
        memcpy(bytes, data->files[i].bytes, data->files[i].size);
        Reader_OpenOwnedBuffer(
            out, bytes, data->files[i].size, &data->readers->allocator,
            nullptr);
        return OK;
    }
    return FAIL("Unknown synthetic resource");
}

static void M_Close(VFS_SOURCE *const source)
{
    SOURCE *const data = (SOURCE *)source;
    for (size_t i = 0; i < ARRAY_SIZE(data->files); i++) {
        Mem_Free(data->files[i].bytes);
    }
    Mem_Free(data);
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

static FILE_ENTRY M_File(
    const char *const path, const uint16_t width, const uint16_t height,
    const uint16_t count, const bool mono)
{
    const size_t header = mono ? 32 : 80;
    const size_t size =
        header + (size_t)width * height * count / (mono ? 8 : 2);
    uint8_t *const bytes = Mem_AllocZ(size);
    memcpy(bytes, "BFNT\x1a", 5);
    bytes[5] = mono ? 0 : 0x83;
    M_U16(bytes + 8, width);
    M_U16(bytes + 10, height);
    M_U16(bytes + 14, count - 1);
    if (mono) {
        bytes[32] = 0x80;
    } else {
        for (size_t i = 0; i < 16; i++) {
            bytes[32 + i * 3] = 0x10;
            bytes[33 + i * 3] = 0x20;
            bytes[34 + i * 3] = 0x30;
        }
        memset(bytes + header, 0x11, size - header);
        bytes[header] = 0x01;
    }
    return (FILE_ENTRY) { path, bytes, size };
}

static void M_Bits(PI_BUILDER *const image, const char *const bits)
{
    for (const char *p = bits; *p != '\0'; p++) {
        if (image->bits % 8 == 0) {
            image->bytes[image->size++] = 0;
        }
        if (*p == '1') {
            image->bytes[image->size - 1] |= 0x80 >> (image->bits % 8);
        }
        image->bits++;
    }
}

static FILE_ENTRY M_Backdrop(const bool wrong_size)
{
    const uint16_t width = wrong_size ? 2 : 256;
    const uint16_t height = wrong_size ? 1 : 80;
    PI_BUILDER image = { .bytes = { 'P', 'i', 0x1a, 0, 0x80, 1, 2, 4, 'P', 'C',
                                    '9', '8', 0, 0 },
                         .size = 18 };
    image.bytes[14] = (uint8_t)(width >> 8);
    image.bytes[15] = (uint8_t)width;
    image.bytes[16] = (uint8_t)(height >> 8);
    image.bytes[17] = (uint8_t)height;
    M_Bits(&image, "01111100100");
    const uint32_t pairs = (uint32_t)width * height / 2;
    int32_t exponent = 0;
    for (uint32_t rest = pairs; rest > 1; rest >>= 1) {
        M_Bits(&image, "1");
        exponent++;
    }
    M_Bits(&image, "0");
    for (int32_t bit = exponent - 1; bit >= 0; bit--) {
        M_Bits(&image, (pairs & (1u << bit)) != 0 ? "1" : "0");
    }
    uint8_t *const bytes = Mem_Alloc(image.size);
    memcpy(bytes, image.bytes, image.size);
    return (FILE_ENTRY) { "eye.pi", bytes, image.size };
}

static SOURCE *M_Source(TEST_ALLOCATOR *const readers)
{
    SOURCE *const source = Mem_AllocZ(sizeof(*source));
    source->base.funcs = &m_Funcs;
    source->base.name = "synthetic gameplay";
    source->readers = readers;
    source->fail_index = -1;
    source->files[0] = M_File("miko.bft", 32, 48, 10, false);
    source->files[1] = M_File("miko32.bft", 32, 32, 38, false);
    source->files[2] = M_File("miko16.bft", 16, 16, 80, false);
    source->files[3] = M_File("mikoft.bft", 16, 16, 256, true);
    source->files[4] = M_Backdrop(false);
    return source;
}

static void M_Empty(const GAMEPLAY_ASSETS *const assets)
{
    const uint8_t *const bytes = (const uint8_t *)assets;
    for (size_t i = 0; i < sizeof(*assets); i++) {
        CHECK_EQ_INT(bytes[i], 0);
    }
}

TEST(cache_outlives_namespace_and_program_and_releases_all_owned_patterns)
{
    TEST_ALLOCATOR readers;
    TEST_ALLOCATOR patterns;
    TestAllocator_Init(&readers);
    TestAllocator_Init(&patterns);
    SOURCE *const source = M_Source(&readers);
    VFS *const files = Vfs_Create(&g_HeapAllocator);
    Vfs_MountSource(files, "", &source->base, 0);
    CHECK(IS_OK(Vfs_MountDir(files, "", getenv("TEST_SPRITE_DIR"), -1, false)));
    GAMEPLAY_ASSETS *const assets = Mem_AllocZ(sizeof(*assets));
    CHECK(Result_Log(
        GameplayAssets_Load(files, &patterns.allocator, assets),
        LOG_LEVEL_ERROR));
    CHECK_EQ_INT(readers.live_blocks, 0);
    CHECK_EQ_INT(readers.size_errors, 0);
    Vfs_Destroy(files);
    CHECK_EQ_INT(assets->patterns.next, 128);
    CHECK_EQ_INT(patterns.live_blocks, 205);
    CHECK_EQ_INT(assets->backdrop.image.width, 256);
    CHECK_EQ_INT(assets->backdrop.image.height, 80);
    for (size_t i = 0; i < 256 * 80; i++) {
        CHECK_EQ_INT(assets->backdrop.image.pixels[i], i % 2 == 0 ? 1 : 14);
    }
    CHECK_EQ_INT(assets->patterns.entries[0].size, 0x0430);
    CHECK_EQ_INT(assets->patterns.entries[10].size, 0x0420);
    CHECK_EQ_INT(assets->patterns.entries[48].size, 0x0210);
    CHECK_EQ_INT(assets->patterns.entries[0].data[0], 0x7f);
    for (uint16_t slot = 48; slot < 128; slot++) {
        CHECK_EQ_INT(assets->patterns.entries[slot].data[0], 0x80);
        CHECK_EQ_INT(assets->patterns.entries[slot].data[1], 1);
        CHECK_EQ_INT(assets->patterns.entries[slot].data[34], 0);
        CHECK_EQ_INT(assets->patterns.entries[slot].data[35], 0);
    }
    CHECK_EQ_INT(assets->gaiji.rows[0][0], 0x8000);
    CHECK_EQ_INT(assets->numbers.rows[12][7], 0);
    CHECK_EQ_INT(assets->sparks.rows[7][7][7], 0x7800);
    CHECK_EQ_INT(assets->bullets.rows[7][7], 0x7800);
    CHECK(assets->has_sprite_palette);
    CHECK_EQ_INT(assets->sprite_palette[3].red, 0x20);
    CHECK_EQ_INT(assets->sprite_palette[3].green, 0x30);
    CHECK_EQ_INT(assets->sprite_palette[3].blue, 0x10);
    GameplayAssets_Free(assets);
    M_Empty(assets);
    GameplayAssets_Free(assets);
    CHECK_EQ_INT(patterns.live_blocks, 0);
    CHECK_EQ_INT(patterns.live_bytes, 0);
    CHECK_EQ_INT(patterns.size_errors, 0);
    Mem_Free(assets);
}

TEST(every_open_and_decode_failure_closes_readers_and_frees_partial_cache)
{
    TEST_ALLOCATOR readers;
    TEST_ALLOCATOR patterns;
    TestAllocator_Init(&readers);
    TestAllocator_Init(&patterns);
    GAMEPLAY_ASSETS *const assets = Mem_AllocZ(sizeof(*assets));
    for (int32_t failure = 0; failure < 11; failure++) {
        SOURCE *const source = M_Source(&readers);
        VFS *const files = Vfs_Create(&g_HeapAllocator);
        Vfs_MountSource(files, "", &source->base, 0);
        CHECK(IS_OK(
            Vfs_MountDir(files, "", getenv("TEST_SPRITE_DIR"), -1, false)));
        if (failure < 4) {
            source->fail_index = failure;
        } else if (failure == 4) {
            source->files[2].size = 31;
        } else if (failure == 5) {
            M_U16(source->files[1].bytes + 8, 24);
        } else if (failure == 6) {
            source->files[2].bytes[80] = 0x12;
            source->files[2].bytes[81] = 0x34;
            source->files[2].bytes[82] = 0x50;
        } else if (failure == 7) {
            M_U16(source->files[3].bytes + 12, 1);
            M_U16(source->files[3].bytes + 14, 256);
        } else if (failure == 8) {
            source->fail_index = 4;
        } else if (failure == 9) {
            source->files[4].size = 2;
        } else {
            Mem_Free(source->files[4].bytes);
            source->files[4] = M_Backdrop(true);
        }
        CHECK(!Result_Ignore(
            GameplayAssets_Load(files, &patterns.allocator, assets)));
        M_Empty(assets);
        CHECK_EQ_INT(patterns.live_blocks, 0);
        CHECK_EQ_INT(patterns.live_bytes, 0);
        CHECK_EQ_INT(readers.live_blocks, 0);
        Vfs_Destroy(files);
    }
    CHECK_EQ_INT(readers.size_errors, 0);
    CHECK_EQ_INT(patterns.size_errors, 0);
    Mem_Free(assets);
}

TEST(backdrop_draw_uses_owned_pixels_and_keeps_live_video_state)
{
    TEST_ALLOCATOR readers;
    TEST_ALLOCATOR owned;
    TestAllocator_Init(&readers);
    TestAllocator_Init(&owned);
    SOURCE *const source = M_Source(&readers);
    VFS *const files = Vfs_Create(&g_HeapAllocator);
    Vfs_MountSource(files, "", &source->base, 0);
    CHECK(IS_OK(Vfs_MountDir(files, "", getenv("TEST_SPRITE_DIR"), -1, false)));
    STAGE_BACKDROP backdrop;
    CHECK(
        Result_Ignore(StageBackdrop_Load(files, &owned.allocator, &backdrop)));
    Vfs_Destroy(files);
    CHECK_EQ_INT(readers.live_blocks, 0);
    backdrop.image.has_position = true;
    backdrop.image.x = 8;
    backdrop.image.y = 16;
    backdrop.image.transparent = 14;
    Video_Init(&g_HeapAllocator);
    memset(g_Video.pages[0], 3, VIDEO_PAGE_SIZE * 2);
    g_Video.drawing_page = 1;
    g_Video.visible_page = 0;
    Video_SetSplit(200, 1234, 5678);
    TestClip_Set(0, 160, 639, 175);

    Grcg_Init();
    const GRCG charger_before = *g_Grcg;

    Palette_Init();
    m_Palette.tone = 50;
    StageBackdrop_Draw(&backdrop);
    CHECK_EQ_INT(m_Palette.tone, 50);
    CHECK_EQ_INT(g_Video.drawing_page, 1);
    CHECK_EQ_INT(g_Video.visible_page, 0);
    CHECK_EQ_INT(g_Video.first_lines, 200);
    CHECK_EQ_INT(g_Video.first_address, 1234);
    CHECK_EQ_INT(g_Video.second_address, 5678);
    CHECK(memcmp(g_Grcg, &charger_before, sizeof(*g_Grcg)) == 0);
    for (size_t i = 0; i < VIDEO_PAGE_SIZE; i++) {
        CHECK_EQ_INT(g_Video.pages[0][i], 3);
        const size_t x = i % VIDEO_WIDTH;
        const size_t y = i / VIDEO_WIDTH;
        const bool drawn = x >= 96 && x < 352 && y >= 160 && y <= 175;
        CHECK_EQ_INT(g_Video.pages[1][i], drawn ? (x % 2 == 0 ? 1 : 14) : 3);
    }
    CHECK_EQ_INT(owned.live_blocks, 1);
    StageBackdrop_Free(&backdrop);
    StageBackdrop_Free(&backdrop);
    CHECK_EQ_INT(owned.live_blocks, 0);
    CHECK_EQ_INT(owned.size_errors, 0);
    Mem_Free(g_Video.pages[0]);
}
