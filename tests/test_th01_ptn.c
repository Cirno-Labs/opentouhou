#include "th01/ptn.h"

#include "core/base/arena.h"

#include "test.h"

#define M_HEADER_SIZE 54
#define M_IMAGE_SIZE 513

static size_t M_File(uint8_t *const bytes, const int32_t count)
{
    const size_t size = M_HEADER_SIZE + (size_t)count * M_IMAGE_SIZE;
    memset(bytes, 0, size);
    memcpy(bytes, "HPTN", 4);
    bytes[4] = 1;
    bytes[5] = count;
    for (int32_t i = 0; i < 16; i++) {
        bytes[6 + i * 3] = i;
        bytes[7 + i * 3] = 15 - i;
        bytes[8 + i * 3] = i / 2;
    }
    for (int32_t image = 0; image < count; image++) {
        uint8_t *const planes =
            bytes + M_HEADER_SIZE + image * M_IMAGE_SIZE + 1;
        for (int32_t y = 0; y < PTN_HEIGHT; y++) {
            for (int32_t x = 0; x < PTN_WIDTH; x++) {
                const uint8_t color = (image + x + y * 3) & 15;
                for (int32_t plane = 0; plane < 4; plane++) {
                    if ((color & (1 << plane)) != 0) {
                        planes[plane * 128 + y * 4 + x / 8] |= 0x80 >> (x % 8);
                    }
                }
            }
        }
    }
    return size;
}

static RESULT M_Load(
    const uint8_t *const bytes, const size_t size,
    const ALLOCATOR *const allocator, PTN *const out)
{
    READER reader;
    Reader_OpenBuffer(&reader, bytes, size, nullptr);
    return Ptn_Load(&reader, allocator, out);
}

TEST(loads_all_colors_in_multiple_planar_images)
{
    ARENA arena;
    Arena_Init(&arena, "PTN test", 1024 * 1024);
    uint8_t bytes[M_HEADER_SIZE + 2 * M_IMAGE_SIZE];
    const size_t size = M_File(bytes, 2);
    PTN sprites;
    CHECK(Result_Log(
        M_Load(bytes, size, Arena_Allocator(&arena), &sprites),
        LOG_LEVEL_ERROR));
    CHECK_EQ_INT(sprites.count, 2);
    if (sprites.pixels != nullptr) {
        for (int32_t image = 0; image < 2; image++) {
            for (int32_t y = 0; y < PTN_HEIGHT; y++) {
                for (int32_t x = 0; x < PTN_WIDTH; x++) {
                    CHECK_EQ_INT(
                        sprites.pixels[image * 1024 + y * 32 + x],
                        (image + x + y * 3) & 15);
                }
            }
        }
    }
    CHECK_EQ_INT(sprites.palette[3].red, 51);
    CHECK_EQ_INT(sprites.palette[3].green, 204);
    CHECK_EQ_INT(sprites.palette[3].blue, 17);
    CHECK_EQ_INT(sprites.palette[15].alpha, 255);
    Arena_Free(&arena);
}

TEST(reads_bytes_most_significant_bit_first_in_b_r_g_e_order)
{
    ARENA arena;
    Arena_Init(&arena, "PTN test", 1024 * 1024);
    uint8_t bytes[M_HEADER_SIZE + M_IMAGE_SIZE];
    const size_t size = M_File(bytes, 1);
    uint8_t *const planes = bytes + M_HEADER_SIZE + 1;
    memset(planes, 0, 512);
    planes[0] = 0x80;
    planes[128 + 1] = 0x40;
    planes[256 + 2] = 0x20;
    planes[384 + 3] = 0x10;
    PTN sprites;
    CHECK(Result_Log(
        M_Load(bytes, size, Arena_Allocator(&arena), &sprites),
        LOG_LEVEL_ERROR));
    if (sprites.pixels != nullptr) {
        for (int32_t x = 0; x < 32; x++) {
            const int32_t expected = x == 0 ? 1
                : x == 9                    ? 2
                : x == 18                   ? 4
                : x == 27                   ? 8
                                            : 0;
            CHECK_EQ_INT(sprites.pixels[x], expected);
        }
    }
    Arena_Free(&arena);
}

TEST(preserves_color_key_and_owns_pixels_after_reader_close)
{
    ARENA arena;
    Arena_Init(&arena, "PTN test", 1024 * 1024);
    uint8_t bytes[M_HEADER_SIZE + M_IMAGE_SIZE];
    const size_t size = M_File(bytes, 1);
    READER reader;
    Reader_OpenBuffer(&reader, bytes, size, nullptr);
    PTN sprites;
    CHECK(Result_Log(
        Ptn_Load(&reader, Arena_Allocator(&arena), &sprites), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(Reader_Remaining(&reader), 0);
    Reader_Close(&reader);
    memset(bytes, 0, sizeof(bytes));
    if (sprites.pixels != nullptr) {
        CHECK_EQ_INT(sprites.pixels[15], PTN_TRANSPARENT);
        CHECK_EQ_INT(sprites.pixels[31], PTN_TRANSPARENT);
        CHECK_EQ_INT(sprites.pixels[32], 3);
    }
    Arena_Free(&arena);
}

TEST(loads_the_full_slot_capacity)
{
    ARENA arena;
    Arena_Init(&arena, "PTN test", 1024 * 1024);
    uint8_t bytes[M_HEADER_SIZE + PTN_MAX_IMAGES * M_IMAGE_SIZE];
    const size_t size = M_File(bytes, PTN_MAX_IMAGES);
    PTN sprites;
    CHECK(Result_Log(
        M_Load(bytes, size, Arena_Allocator(&arena), &sprites),
        LOG_LEVEL_ERROR));
    CHECK_EQ_INT(sprites.count, PTN_MAX_IMAGES);
    if (sprites.pixels != nullptr) {
        CHECK_EQ_INT(sprites.pixels[63 * 1024 + 31 * 32 + 31], 11);
    }
    Arena_Free(&arena);
}

TEST(rejects_every_truncated_prefix)
{
    ARENA arena;
    Arena_Init(&arena, "PTN test", 1024 * 1024);
    uint8_t bytes[M_HEADER_SIZE + 2 * M_IMAGE_SIZE];
    const size_t size = M_File(bytes, 2);
    for (size_t prefix = 0; prefix < size; prefix++) {
        PTN sprites;
        CHECK(!Result_Ignore(
            M_Load(bytes, prefix, Arena_Allocator(&arena), &sprites)));
        CHECK(sprites.pixels == nullptr);
        CHECK_EQ_INT(arena.used, 0);
        CHECK_EQ_INT(sprites.count, 0);
    }
    Arena_Free(&arena);
}

TEST(rejects_bad_signature_signed_counts_and_palette)
{
    ARENA arena;
    Arena_Init(&arena, "PTN test", 1024 * 1024);
    const int32_t offsets[] = { 0, 5, 5, 5, 6, 53 };
    const uint8_t values[] = { 'X', 0, 65, 0xff, 16, 0xff };
    for (size_t i = 0; i < sizeof(offsets) / sizeof(offsets[0]); i++) {
        uint8_t bytes[M_HEADER_SIZE + M_IMAGE_SIZE];
        const size_t size = M_File(bytes, 1);
        bytes[offsets[i]] = values[i];
        PTN sprites;
        CHECK(!Result_Ignore(
            M_Load(bytes, size, Arena_Allocator(&arena), &sprites)));
        CHECK(sprites.pixels == nullptr);
        CHECK_EQ_INT(arena.used, 0);
    }
    Arena_Free(&arena);
}

TEST(assets_share_the_caller_arena_and_release_together)
{
    ARENA arena;
    Arena_Init(&arena, "PTN assets", 4096);
    uint8_t bytes[M_HEADER_SIZE + M_IMAGE_SIZE];
    const size_t size = M_File(bytes, 1);
    PTN first, second;
    CHECK(Result_Ignore(M_Load(bytes, size, Arena_Allocator(&arena), &first)));
    CHECK(Result_Ignore(M_Load(bytes, size, Arena_Allocator(&arena), &second)));
    memset(bytes, 0, sizeof(bytes));
    CHECK(first.pixels != second.pixels);
    CHECK_EQ_INT(first.pixels[15], PTN_TRANSPARENT);
    CHECK_EQ_INT(second.pixels[31], PTN_TRANSPARENT);
    CHECK_EQ_INT(arena.used, 2 * PTN_WIDTH * PTN_HEIGHT);
    Arena_Reset(&arena);
    CHECK_EQ_INT(arena.used, 0);
    Arena_Free(&arena);
}
