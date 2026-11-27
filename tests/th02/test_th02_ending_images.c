#include "tests/test.h"
#include "tests/test_allocator.h"
#include "tests/th02/th02_clip_fixture.h"
#include "tests/th02/th02_palette_fixture.h"
#include "th02/ending/images.h"

typedef struct {
    const char *path;
    uint8_t *bytes;
    size_t size;
} FILE_ENTRY;

typedef struct {
    VFS_SOURCE base;
    FILE_ENTRY files[ENDING_IMAGE_COUNT + ENDING_COLOR_COUNT];
    TEST_ALLOCATOR *readers;
    int32_t failure;
} SOURCE;

typedef struct {
    uint8_t bytes[64];
    size_t size;
    size_t bits;
} PI_BUILDER;

static const char *const m_Names[] = {
    "ed01.pi",   "ed02.pi",   "ed03.pi",   "ed04.pi",   "ed05.pi",
    "ed06.pi",   "ed07.pi",   "ed08.pi",   "ed09.pi",   "all.pi",
    "but.pi",    "extra.pi",  "ed06b.rgb", "ed06c.rgb", "ed07a.rgb",
    "ed07b.rgb", "ed08a.rgb", "ed08b.rgb", "ed08c.rgb", "ed03a.rgb",
};

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
        FAIL_IF((int32_t)i == data->failure, "Synthetic open failure");
        const FILE_ENTRY *const entry = &data->files[i];
        uint8_t *const bytes =
            Allocator_Alloc(&data->readers->allocator, entry->size);
        memcpy(bytes, entry->bytes, entry->size);
        Reader_OpenOwnedBuffer(
            out, bytes, entry->size, &data->readers->allocator, nullptr);
        return OK;
    }
    return FAIL("Unknown resource");
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

static FILE_ENTRY M_Image(const size_t id, const bool wrong_size)
{
    const uint16_t width = wrong_size ? 2
        : id == 10                    ? 288
        : id == 11                    ? 320
                                      : 640;
    const uint16_t height = wrong_size ? 1 : id >= 10 ? 160 : 400;
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
    return (FILE_ENTRY) { m_Names[id], bytes, image.size };
}

static SOURCE *M_Source(TEST_ALLOCATOR *const readers)
{
    SOURCE *const source = Mem_AllocZ(sizeof(*source));
    source->base.funcs = &m_Funcs;
    source->base.name = "synthetic ending";
    source->readers = readers;
    source->failure = -1;
    for (size_t i = 0; i < ENDING_IMAGE_COUNT; i++) {
        source->files[i] = M_Image(i, false);
    }
    for (size_t i = ENDING_IMAGE_COUNT; i < ARRAY_SIZE(source->files); i++) {
        uint8_t *const bytes = Mem_Alloc(53);
        for (size_t j = 0; j < 53; j++) {
            bytes[j] = (uint8_t)(j + i);
        }
        source->files[i] = (FILE_ENTRY) { m_Names[i], bytes, 53 };
    }
    return source;
}

static VFS *M_Mount(SOURCE *const source)
{
    VFS *const files = Vfs_Create(&g_HeapAllocator);
    Vfs_MountSource(files, "", &source->base, 0);
    return files;
}

TEST(images_and_color_bytes_outlive_the_namespace_and_release_exact_sizes)
{
    TEST_ALLOCATOR readers;
    TEST_ALLOCATOR owned;
    TestAllocator_Init(&readers);
    TestAllocator_Init(&owned);
    SOURCE *const source = M_Source(&readers);
    source->files[12].size = 0;
    source->files[13].size = 3;
    VFS *const files = M_Mount(source);
    ENDING_IMAGES images;
    CHECK(Result_Ignore(EndingImages_Load(files, &owned.allocator, &images)));
    Vfs_Destroy(files);
    CHECK_EQ_INT(readers.live_blocks, 0);
    CHECK_EQ_INT(readers.size_errors, 0);
    CHECK_EQ_INT(owned.live_blocks, 12);
    for (size_t i = 0; i < ENDING_IMAGE_COUNT; i++) {
        const PI_IMAGE *const image = &images.images[i];
        CHECK_EQ_INT(image->width, i == 10 ? 288 : i == 11 ? 320 : 640);
        CHECK_EQ_INT(image->height, i >= 10 ? 160 : 400);
        for (size_t j = 0; j < (size_t)image->width * image->height; j++) {
            CHECK_EQ_INT(image->pixels[j], j % 2 == 0 ? 1 : 14);
        }
    }
    CHECK_EQ_INT(images.colors[0].size, 0);
    CHECK_EQ_INT(images.colors[1].size, 3);
    for (size_t i = 2; i < ENDING_COLOR_COUNT; i++) {
        CHECK_EQ_INT(images.colors[i].size, 48);
        CHECK_EQ_INT(images.colors[i].bytes[47], 47 + 12 + i);
    }
    EndingImages_Free(&images);
    EndingImages_Free(&images);
    CHECK_EQ_INT(owned.live_blocks, 0);
    CHECK_EQ_INT(owned.size_errors, 0);
}

TEST(all_open_decode_and_dimension_failures_release_partial_images)
{
    TEST_ALLOCATOR readers;
    TEST_ALLOCATOR owned;
    TestAllocator_Init(&readers);
    TestAllocator_Init(&owned);
    for (int32_t failure = 0; failure < 44; failure++) {
        SOURCE *const source = M_Source(&readers);
        if (failure < 20) {
            source->failure = failure;
        } else if (failure < 32) {
            source->files[failure - 20].size = 2;
        } else {
            const size_t id = failure - 32;
            Mem_Free(source->files[id].bytes);
            source->files[id] = M_Image(id, true);
        }
        VFS *const files = M_Mount(source);
        ENDING_IMAGES images;
        CHECK(!Result_Ignore(
            EndingImages_Load(files, &owned.allocator, &images)));
        const uint8_t *const bytes = (const uint8_t *)&images;
        for (size_t i = 0; i < sizeof(images); i++) {
            CHECK_EQ_INT(bytes[i], 0);
        }
        CHECK_EQ_INT(owned.live_blocks, 0);
        CHECK_EQ_INT(readers.live_blocks, 0);
        Vfs_Destroy(files);
    }
    CHECK_EQ_INT(owned.size_errors, 0);
    CHECK_EQ_INT(readers.size_errors, 0);
}

TEST(drawing_and_palette_changes_are_separate_and_keep_live_state)
{
    TEST_ALLOCATOR readers;
    TEST_ALLOCATOR owned;
    TestAllocator_Init(&readers);
    TestAllocator_Init(&owned);
    VFS *const files = M_Mount(M_Source(&readers));
    ENDING_IMAGES images;
    CHECK(Result_Ignore(EndingImages_Load(files, &owned.allocator, &images)));
    Vfs_Destroy(files);
    Video_Init(&g_HeapAllocator);
    g_Video.drawing_page = 1;
    g_Video.visible_page = 0;
    Video_SetSplit(200, 1234, 5678);
    TestClip_Set(0, 0, 639, 399);

    Grcg_Init();

    GFX_COLOR initial[16];
    memcpy(initial, g_Video.palette, sizeof(initial));
    for (size_t id = 0; id < ENDING_IMAGE_COUNT; id++) {
        memset(g_Video.pages[0], 3, VIDEO_PAGE_SIZE * 2);
        images.images[id].transparent = 14;
        images.images[id].x = images.images[id].y = 16;
        EndingImages_Draw(&images, (ENDING_IMAGE)id);
        CHECK(memcmp(initial, g_Video.palette, sizeof(initial)) == 0);
        for (size_t i = 0; i < VIDEO_PAGE_SIZE; i++) {
            CHECK_EQ_INT(g_Video.pages[0][i], 3);
            const size_t x = i % 640;
            const size_t y = i / 640;
            const bool drawn = x < (size_t)images.images[id].width
                && y < (size_t)images.images[id].height;
            CHECK_EQ_INT(
                g_Video.pages[1][i], drawn ? (x % 2 == 0 ? 1 : 14) : 3);
        }
    }

    Palette_Init();
    m_Palette.tone = 76;
    EndingImages_ApplyPalette(&images, ENDING_IMAGE_CREDITS_3);
    memset(m_Palette.colors, 0x23, sizeof(m_Palette.colors));
    images.colors[0].size = 3;
    memcpy(images.colors[0].bytes, (uint8_t[]) { 2, 0x1a, 0xe2 }, 3);
    EndingImages_Recolor(&images, ENDING_COLOR_CREDITS_1_B);
    CHECK_EQ_INT(m_Palette.colors[0][0], 0x22);
    CHECK_EQ_INT(m_Palette.colors[0][1], 0xba);
    CHECK_EQ_INT(m_Palette.colors[0][2], 0xe2);
    CHECK_EQ_INT(m_Palette.colors[1][0], 0x33);
    CHECK_EQ_INT(m_Palette.tone, 76);
    CHECK_EQ_INT(g_Video.drawing_page, 1);
    CHECK_EQ_INT(g_Video.visible_page, 0);
    CHECK_EQ_INT(g_Video.first_lines, 200);
    CHECK_EQ_INT(g_Video.first_address, 1234);
    CHECK_EQ_INT(g_Video.second_address, 5678);
    EndingImages_Free(&images);
    Mem_Free(g_Video.pages[0]);
    CHECK_EQ_INT(owned.live_blocks, 0);
}
