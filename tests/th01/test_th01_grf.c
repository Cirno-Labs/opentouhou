#include "tests/test.h"
#include "th01/grf.h"

#include "core/base/arena.h"

#define M_PACKED_SIZE (GRF_WIDTH * GRF_HEIGHT / 8)

typedef struct {
    uint8_t bytes[8192];
    size_t size;
    size_t decoded;
    uint8_t run_byte;
} GRF_BUILDER;

static GRF_BUILDER M_Header(const uint8_t run_byte)
{
    GRF_BUILDER file = { .size = 64, .run_byte = run_byte };
    memcpy(file.bytes, "HGRF", 4);
    file.bytes[4] = 1;
    file.bytes[5] = run_byte;
    memset(file.bytes + 16, 0xff, 48);
    return file;
}

static void M_Literal(GRF_BUILDER *const file, const uint8_t value)
{
    file->bytes[file->size++] = value;
    if (value == file->run_byte) {
        file->bytes[file->size++] = 0;
    }
    file->decoded++;
}

static void M_Run(
    GRF_BUILDER *const file, const uint8_t count, const uint8_t value)
{
    file->bytes[file->size++] = file->run_byte;
    file->bytes[file->size++] = count;
    file->bytes[file->size++] = value;
    file->decoded += (size_t)count + 1;
}

static void M_Seal(GRF_BUILDER *const file)
{
    const size_t encoded_size = file->size - 64;
    file->bytes[8] = encoded_size & 255;
    file->bytes[9] = encoded_size >> 8;
}

static void M_FinishZero(GRF_BUILDER *const file)
{
    while (file->decoded < M_PACKED_SIZE) {
        const size_t remaining = M_PACKED_SIZE - file->decoded;
        if (remaining == 1) {
            M_Literal(file, 0);
        } else {
            M_Run(file, remaining > 256 ? 255 : remaining - 1, 0);
        }
    }
    M_Seal(file);
}

static RESULT M_Load(
    const GRF_BUILDER *const file, const size_t size,
    const ALLOCATOR *const allocator, GRF *const out)
{
    READER reader;
    Reader_OpenBuffer(&reader, file->bytes, size, nullptr);
    return Grf_Load(&reader, allocator, out);
}

TEST(decodes_literal_masks_with_most_significant_bit_first)
{
    ARENA arena;
    Arena_Init(&arena, "GRF test", 65536);
    GRF_BUILDER file = M_Header(10);
    M_Literal(&file, 0x80);
    M_Literal(&file, 0x01);
    M_Literal(&file, 0x5a);
    M_FinishZero(&file);
    GRF mask;
    CHECK(Result_Log(
        M_Load(&file, file.size, Arena_Allocator(&arena), &mask),
        LOG_LEVEL_ERROR));
    const uint8_t expected[] = {
        1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0,
    };
    CHECK(memcmp(mask.coverage, expected, sizeof(expected)) == 0);
    for (size_t pixel = sizeof(expected); pixel < GRF_WIDTH * GRF_HEIGHT;
         pixel++) {
        CHECK_EQ_INT(mask.coverage[pixel], 0);
    }
    Arena_Free(&arena);
}

TEST(custom_command_zero_count_emits_the_command_byte)
{
    ARENA arena;
    Arena_Init(&arena, "GRF test", 65536);
    GRF_BUILDER file = M_Header(0xa5);
    M_Literal(&file, 0xa5);
    M_FinishZero(&file);
    GRF mask;
    CHECK(Result_Ignore(
        M_Load(&file, file.size, Arena_Allocator(&arena), &mask)));
    const uint8_t expected[] = { 1, 0, 1, 0, 0, 1, 0, 1 };
    CHECK(memcmp(mask.coverage, expected, sizeof(expected)) == 0);
    CHECK_EQ_INT(mask.coverage[8], 0);
    Arena_Free(&arena);
}

TEST(a_nonzero_count_emits_one_more_byte_than_the_count)
{
    ARENA arena;
    Arena_Init(&arena, "GRF test", 65536);
    GRF_BUILDER file = M_Header(10);
    M_Run(&file, 1, 0x81);
    M_Literal(&file, 0x40);
    M_FinishZero(&file);
    GRF mask;
    CHECK(Result_Ignore(
        M_Load(&file, file.size, Arena_Allocator(&arena), &mask)));
    CHECK_EQ_INT(mask.coverage[0], 1);
    CHECK_EQ_INT(mask.coverage[7], 1);
    CHECK_EQ_INT(mask.coverage[8], 1);
    CHECK_EQ_INT(mask.coverage[15], 1);
    CHECK_EQ_INT(mask.coverage[16], 0);
    CHECK_EQ_INT(mask.coverage[17], 1);
    Arena_Free(&arena);
}

TEST(clips_the_final_zero_run_at_the_hud_boundary)
{
    ARENA arena;
    Arena_Init(&arena, "GRF test", 65536);
    GRF_BUILDER file = M_Header(10);
    while (file.decoded < 5120) {
        M_Run(&file, 255, 0);
    }
    M_Run(&file, 255, 0);
    M_Seal(&file);
    CHECK_EQ_INT(file.decoded, 5376);
    GRF mask;
    CHECK(Result_Ignore(
        M_Load(&file, file.size, Arena_Allocator(&arena), &mask)));
    CHECK_EQ_INT(arena.used, GRF_WIDTH * GRF_HEIGHT);
    for (size_t pixel = 0; pixel < GRF_WIDTH * GRF_HEIGHT; pixel++) {
        CHECK_EQ_INT(mask.coverage[pixel], 0);
    }
    Arena_Free(&arena);
}

TEST(keeps_unused_planes_and_palette_out_of_the_mask)
{
    ARENA arena;
    Arena_Init(&arena, "GRF test", 65536);
    GRF_BUILDER file = M_Header(0);
    M_Literal(&file, 0x80);
    M_FinishZero(&file);
    file.bytes[10] = 1;
    file.bytes[file.size++] = 0xff;
    READER reader;
    Reader_OpenBuffer(&reader, file.bytes, file.size, nullptr);
    GRF mask;
    CHECK(Result_Ignore(Grf_Load(&reader, Arena_Allocator(&arena), &mask)));
    CHECK_EQ_INT(Reader_Remaining(&reader), 1);
    CHECK_EQ_INT(Reader_ReadU8(&reader), 0xff);
    Reader_Close(&reader);
    memset(file.bytes, 0, sizeof(file.bytes));
    CHECK_EQ_INT(mask.coverage[0], 1);
    CHECK_EQ_INT(mask.coverage[1], 0);
    Arena_Free(&arena);
}

TEST(rejects_every_truncated_prefix_without_using_the_arena)
{
    ARENA arena;
    Arena_Init(&arena, "GRF test", 65536);
    GRF_BUILDER file = M_Header(10);
    M_Literal(&file, 0xff);
    M_FinishZero(&file);
    for (size_t prefix = 0; prefix < file.size; prefix++) {
        GRF mask;
        CHECK(!Result_Ignore(
            M_Load(&file, prefix, Arena_Allocator(&arena), &mask)));
        CHECK(mask.coverage == nullptr);
        CHECK_EQ_INT(arena.used, 0);
    }
    Arena_Free(&arena);
}

TEST(rejects_short_runs_and_short_decoded_masks_without_using_the_arena)
{
    ARENA arena;
    Arena_Init(&arena, "GRF test", 65536);
    for (int32_t length = 0; length < 4; length++) {
        GRF_BUILDER file = M_Header(10);
        const uint8_t stream[] = { 10, 255, 0, 10 };
        memcpy(file.bytes + 64, stream, length);
        file.size += length;
        M_Seal(&file);
        GRF mask;
        CHECK(!Result_Ignore(
            M_Load(&file, file.size, Arena_Allocator(&arena), &mask)));
        CHECK(mask.coverage == nullptr);
        CHECK_EQ_INT(arena.used, 0);
    }
    Arena_Free(&arena);
}

TEST(rejects_bad_magic)
{
    ARENA arena;
    Arena_Init(&arena, "GRF test", 65536);
    GRF_BUILDER file = M_Header(10);
    M_FinishZero(&file);
    file.bytes[0] = 'X';
    GRF mask;
    CHECK(!Result_Ignore(
        M_Load(&file, file.size, Arena_Allocator(&arena), &mask)));
    CHECK(mask.coverage == nullptr);
    CHECK_EQ_INT(arena.used, 0);
    Arena_Free(&arena);
}

TEST(reads_plane_sizes_of_spaces_up_to_the_end_of_the_file)
{
    ARENA arena;
    Arena_Init(&arena, "GRF test", 65536);
    GRF_BUILDER file = M_Header(10);
    M_Literal(&file, 0x80);
    M_FinishZero(&file);
    memset(file.bytes + 8, ' ', 8);
    GRF mask;
    CHECK(Result_Log(
        M_Load(&file, file.size, Arena_Allocator(&arena), &mask),
        LOG_LEVEL_ERROR));
    CHECK_EQ_INT(mask.coverage[0], 1);
    CHECK_EQ_INT(mask.coverage[1], 0);
    Arena_Free(&arena);
}

TEST(masks_share_the_asset_arena_and_release_together)
{
    ARENA arena;
    Arena_Init(&arena, "GRF assets", 128 * 1024);
    GRF_BUILDER file = M_Header(10);
    M_Literal(&file, 0x80);
    M_FinishZero(&file);
    GRF first;
    GRF second;
    CHECK(Result_Ignore(
        M_Load(&file, file.size, Arena_Allocator(&arena), &first)));
    CHECK(Result_Ignore(
        M_Load(&file, file.size, Arena_Allocator(&arena), &second)));
    CHECK(first.coverage != second.coverage);
    memset(file.bytes, 0, sizeof(file.bytes));
    CHECK_EQ_INT(first.coverage[0], 1);
    CHECK_EQ_INT(second.coverage[0], 1);
    CHECK_EQ_INT(arena.used, 2 * GRF_WIDTH * GRF_HEIGHT);
    Arena_Reset(&arena);
    CHECK_EQ_INT(arena.used, 0);
    Arena_Free(&arena);
}
