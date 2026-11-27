#include "tests/test.h"
#include "tests/test_allocator.h"
#include "tests/th02/th02_sprites.h"
#include "th02/bomb/assets.h"
#include "th02/bomb/tail.h"

#include "core/base/memory.h"
#include "core/io/reader.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *path;
    uint8_t *bytes;
    size_t size;
} FILE_ENTRY;

typedef struct {
    VFS_SOURCE base;
    FILE_ENTRY files[3];
    uint32_t opened[3];
    TEST_ALLOCATOR *readers;
    int32_t fail_index;
} SOURCE;

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
        data->opened[i]++;
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

static FILE_ENTRY M_Payload(
    const char *const path, const size_t offset, const size_t size)
{
    uint8_t *const bytes = Mem_Alloc(offset + size);
    memset(bytes, 0xe7, offset);
    for (size_t i = 0; i < size; i++) {
        bytes[offset + i] = (uint8_t)(i * 73 + 19);
    }
    return (FILE_ENTRY) { path, bytes, offset + size };
}

typedef struct {
    uint8_t bytes[128];
    size_t size;
    size_t bits;
} PI_BUILDER;

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

static FILE_ENTRY M_Image(const uint8_t type, const bool wrong_size)
{
    static const char *const names[] = { "bomb1.pi", "bomb2.pi", "bomb3.pi" };
    static const uint16_t widths[] = { 224, 384, 384 };
    static const uint16_t heights[] = { 216, 232, 368 };
    const uint16_t width = wrong_size ? 2 : widths[type];
    const uint16_t height = wrong_size ? 1 : heights[type];
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
    return (FILE_ENTRY) { names[type], bytes, image.size };
}

static SOURCE *M_Source(
    TEST_ALLOCATOR *const readers, const uint8_t type, const bool wrong_size)
{
    SOURCE *const source = Mem_AllocZ(sizeof(*source));
    source->base.funcs = &m_Funcs;
    source->base.name = "synthetic bombs";
    source->readers = readers;
    source->fail_index = -1;
    source->files[0] = M_Payload("bombs.bft", 32, 1080);
    source->files[1] = M_Image(type, wrong_size);
    source->files[2] = M_Payload("bomb1.bft", 80, 5184);
    return source;
}

static void M_Empty(const BOMB_ASSETS *const assets)
{
    const uint8_t *const bytes = (const uint8_t *)assets;
    for (size_t i = 0; i < sizeof(*assets); i++) {
        CHECK_EQ_INT(bytes[i], 0);
    }
}

static uint16_t M_FontRow(
    const void *const context, const uint16_t code, const bool wide,
    const uint8_t y)
{
    CHECK_EQ_INT(code, 0x56);
    CHECK(!wide && y < 16);
    const uint8_t *const rows = context;
    return (uint16_t)rows[y] << 8;
}

TEST(tail_holds_the_gaiji_backup_block_after_the_masks)
{
    uint8_t rows[16];
    uint8_t tail[BOMB_TAIL_SIZE];
    for (uint8_t i = 0; i < 16; i++) {
        rows[i] = (uint8_t)(i * 7 + 1);
    }
    const TRAM_FONT font = { M_FontRow, rows };
    for (int32_t demo = 0; demo < 2; demo++) {
        memset(tail, 0xab, sizeof(tail));
        BombTail_Build(&font, demo != 0, tail);
        const uint16_t next = demo != 0 ? 0x6f87 : 0x72f3;
        for (size_t i = 0; i < 24; i++) {
            CHECK_EQ_INT(
                tail[i],
                i == 8        ? 1
                    : i == 10 ? next & 0xff
                    : i == 11 ? next >> 8
                              : 0);
        }
        for (size_t i = 0; i < 16; i++) {
            CHECK_EQ_INT(tail[24 + i * 2], rows[i]);
            CHECK_EQ_INT(tail[25 + i * 2], rows[i]);
            CHECK_EQ_INT(tail[56 + i], 0);
        }
    }
}

TEST(selected_cache_keeps_owned_pixels_masks_and_optional_text_after_close)
{
    static const int32_t widths[] = { 224, 384, 384 };
    static const int32_t heights[] = { 216, 232, 368 };
    for (uint8_t type = 0; type < 3; type++) {
        TEST_ALLOCATOR readers;
        TEST_ALLOCATOR pixels;
        TestAllocator_Init(&readers);
        TestAllocator_Init(&pixels);
        SOURCE *const source = M_Source(&readers, type, false);
        VFS *const files = Vfs_Create(&g_HeapAllocator);
        Vfs_MountSource(files, "", &source->base, 0);
        CHECK(IS_OK(
            Vfs_MountDir(files, "", getenv("TEST_SPRITE_DIR"), -1, false)));
        BOMB_ASSETS assets;
        CHECK(Result_Log(
            BombAssets_Load(files, type, &pixels.allocator, &assets),
            LOG_LEVEL_ERROR));
        CHECK_EQ_INT(readers.live_blocks, 0);
        CHECK_EQ_INT(source->opened[0], 1);
        CHECK_EQ_INT(source->opened[1], 1);
        CHECK_EQ_INT(source->opened[2], type == 1 ? 1 : 0);
        Vfs_Destroy(files);
        CHECK_EQ_INT(assets.shot_type, type);
        CHECK_EQ_INT(assets.illustration.width, widths[type]);
        CHECK_EQ_INT(assets.illustration.height, heights[type]);
        CHECK_EQ_INT(assets.illustration.transparent, -1);
        CHECK(!assets.illustration.has_palette);
        CHECK_EQ_INT(pixels.live_blocks, 1);
        for (size_t i = 0; i < (size_t)widths[type] * heights[type]; i++) {
            CHECK_EQ_INT(assets.illustration.pixels[i], i % 2 == 0 ? 1 : 14);
        }
        static const uint8_t particles[5][8] = {
            { 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00 },
            { 0x00, 0x00, 0x10, 0x38, 0x10, 0x00, 0x00, 0x00 },
            { 0x00, 0x10, 0x10, 0x7c, 0x10, 0x10, 0x00, 0x00 },
            { 0x10, 0x10, 0x38, 0xfe, 0x38, 0x10, 0x10, 0x00 },
            { 0x3c, 0x7e, 0xff, 0xff, 0xff, 0xff, 0x7e, 0x3c },
        };
        CHECK(memcmp(assets.particles, particles, sizeof(particles)) == 0);
        for (size_t i = 0; i < sizeof(assets.transition); i++) {
            CHECK_EQ_INT(
                ((uint8_t *)assets.transition)[i], (uint8_t)(i * 73 + 19));
        }
        for (size_t i = 0; i < sizeof(assets.text); i++) {
            CHECK_EQ_INT(
                ((uint8_t *)assets.text)[i],
                type == 1 ? (uint8_t)(i * 73 + 19) : 0);
        }
        BombAssets_Free(&assets);
        M_Empty(&assets);
        BombAssets_Free(&assets);
        CHECK_EQ_INT(pixels.live_blocks, 0);
        CHECK_EQ_INT(pixels.size_errors + readers.size_errors, 0);
    }
}

TEST(failures_close_readers_and_free_decoded_images_without_publishing_a_cache)
{
    for (int32_t failure = 0; failure < 8; failure++) {
        TEST_ALLOCATOR readers;
        TEST_ALLOCATOR pixels;
        TestAllocator_Init(&readers);
        TestAllocator_Init(&pixels);
        SOURCE *const source = M_Source(&readers, 1, failure == 6);
        VFS *const files = Vfs_Create(&g_HeapAllocator);
        Vfs_MountSource(files, "", &source->base, 0);
        if (failure != 7) {
            CHECK(IS_OK(
                Vfs_MountDir(files, "", getenv("TEST_SPRITE_DIR"), -1, false)));
        }
        if (failure < 3) {
            source->fail_index = failure;
        } else if (failure == 3) {
            source->files[0].size--;
        } else if (failure == 4) {
            source->files[2].size--;
        } else if (failure == 5) {
            source->files[1].size = 2;
        }
        BOMB_ASSETS assets;
        CHECK(!Result_Ignore(
            BombAssets_Load(files, 1, &pixels.allocator, &assets)));
        M_Empty(&assets);
        CHECK_EQ_INT(pixels.live_blocks, 0);
        CHECK_EQ_INT(readers.live_blocks, 0);
        CHECK_EQ_INT(pixels.size_errors + readers.size_errors, 0);
        Vfs_Destroy(files);
    }
}
