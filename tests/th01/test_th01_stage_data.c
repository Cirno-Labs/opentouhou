#include "tests/test.h"
#include "th01/stage_data.h"

#include "core/base/arena.h"
#include "core/io/file.h"

#define M_SIZE (22 + 5 * 250)

typedef struct {
    ARENA arena;
    size_t allocations;
} STORAGE;

static void *M_Alloc(void *const context, const size_t size)
{
    STORAGE *const storage = context;
    storage->allocations++;
    return Arena_Alloc(&storage->arena, size);
}

static ALLOCATOR M_Init(STORAGE *const storage)
{
    *storage = (STORAGE) {};
    Arena_Init(&storage->arena, "Stage layouts", 65536);
    return (ALLOCATOR) { .alloc = M_Alloc, .context = storage };
}

static RESULT M_Load(
    const uint8_t *const bytes, const size_t size,
    const ALLOCATOR *const allocator, STAGE_DATA *const out)
{
    READER reader;
    Reader_OpenBuffer(&reader, bytes, size, nullptr);
    return StageData_Load(&reader, allocator, out);
}

TEST(expands_low_nibbles_from_bit_three_at_the_left)
{
    uint8_t bytes[M_SIZE] = {};
    const uint8_t packed[] = { 0xf8, 0xe1, 0xd2, 0xc4, 0xba };
    memcpy(bytes + 22, packed, sizeof(packed));
    const bool expected[] = {
        true, false, false, false, false, false, false, true,  false, false,
        true, false, false, true,  false, false, true,  false, true,  false,
    };
    STORAGE storage;
    const ALLOCATOR allocator = M_Init(&storage);
    STAGE_DATA data;
    CHECK(Result_Log(
        M_Load(bytes, sizeof(bytes), &allocator, &data), LOG_LEVEL_ERROR));
    for (int32_t column = 0; column < STAGE_DATA_COLUMNS; column++) {
        CHECK(data.stages[0].cells[0][column].card == expected[column]);
        CHECK_EQ_INT(data.stages[0].cells[0][column].card_extra_hp, 0);
    }
    CHECK(!data.stages[0].cells[1][0].card);
    CHECK_EQ_INT(storage.allocations, 1);
    CHECK_EQ_INT(storage.arena.used, 5 * sizeof(STAGE_DATA_STAGE));
    Arena_Free(&storage.arena);
}

TEST(reads_five_records_without_ram_padding_and_keeps_the_boss_layout)
{
    uint8_t bytes[M_SIZE] = {};
    for (int32_t stage = 0; stage < 5; stage++) {
        const int32_t offset = 22 + stage * 250;
        bytes[offset + stage * 5 + stage] = 8;
        bytes[offset + 49] = 1;
        bytes[offset + 50 + stage * 20 + stage * 4] = stage + 17;
        bytes[offset + 249] = stage + 1;
    }
    STORAGE storage;
    const ALLOCATOR allocator = M_Init(&storage);
    STAGE_DATA data;
    CHECK(Result_Ignore(M_Load(bytes, sizeof(bytes), &allocator, &data)));
    for (int32_t stage = 0; stage < 5; stage++) {
        for (int32_t row = 0; row < 10; row++) {
            for (int32_t column = 0; column < 20; column++) {
                const STAGE_DATA_CELL cell =
                    data.stages[stage].cells[row][column];
                const bool first = row == stage && column == stage * 4;
                const bool last = row == 9 && column == 19;
                CHECK(cell.card == (first || last));
                CHECK_EQ_INT(cell.card_extra_hp, 0);
                CHECK_EQ_INT(
                    cell.obstacle,
                    first      ? stage + 17
                        : last ? stage + 1
                               : 0);
            }
        }
    }
    Arena_Free(&storage.arena);
}

TEST(normalizes_every_signed_obstacle_value_and_allows_card_overlap)
{
    uint8_t bytes[M_SIZE] = {};
    for (int32_t value = 0; value < 256; value++) {
        const int32_t stage = value / 200;
        const int32_t cell = value % 200;
        const int32_t offset = 22 + stage * 250;
        bytes[offset + cell / 4] |= 8 >> (cell % 4);
        bytes[offset + 50 + cell] = value;
    }
    // Repeat HP markers without cards in the second record.
    bytes[22 + 250 + 50 + 60] = 14;
    bytes[22 + 250 + 50 + 61] = 15;
    bytes[22 + 250 + 50 + 62] = 16;
    STORAGE storage;
    const ALLOCATOR allocator = M_Init(&storage);
    STAGE_DATA data;
    CHECK(Result_Ignore(M_Load(bytes, sizeof(bytes), &allocator, &data)));
    for (int32_t value = 0; value < 256; value++) {
        const int32_t index = value % 200;
        const STAGE_DATA_CELL cell =
            data.stages[value / 200].cells[index / 20][index % 20];
        CHECK(cell.card);
        CHECK_EQ_INT(
            cell.card_extra_hp, value >= 14 && value <= 16 ? value - 13 : 0);
        const bool physical =
            (value >= 1 && value <= 13) || (value >= 17 && value <= 21);
        CHECK_EQ_INT(cell.obstacle, physical ? value : 0);
    }
    for (int32_t column = 0; column < 3; column++) {
        const STAGE_DATA_CELL cell = data.stages[1].cells[3][column];
        CHECK(!cell.card);
        CHECK_EQ_INT(cell.card_extra_hp, 0);
        CHECK_EQ_INT(cell.obstacle, STAGE_DATA_OBSTACLE_NONE);
    }
    Arena_Free(&storage.arena);
}

TEST(ignores_all_header_fields_and_preserves_reader_boundaries)
{
    uint8_t bytes[7 + M_SIZE + 3];
    memset(bytes, 0xa5, sizeof(bytes));
    memset(bytes + 7 + 22, 0, M_SIZE - 22);
    bytes[7 + 22] = 8;
    READER reader;
    Reader_OpenBuffer(&reader, bytes, sizeof(bytes), nullptr);
    Reader_Skip(&reader, 7);
    STORAGE storage;
    const ALLOCATOR allocator = M_Init(&storage);
    STAGE_DATA data;
    CHECK(Result_Ignore(StageData_Load(&reader, &allocator, &data)));
    CHECK_EQ_INT(Reader_Tell(&reader), 7 + M_SIZE);
    CHECK_EQ_INT(Reader_Remaining(&reader), 3);
    CHECK_EQ_INT(Reader_ReadU8(&reader), 0xa5);
    Reader_Close(&reader);
    memset(bytes, 0, sizeof(bytes));
    CHECK(data.stages[0].cells[0][0].card);
    CHECK_EQ_INT(storage.allocations, 1);
    Arena_Free(&storage.arena);
}

TEST(rejects_every_truncated_prefix_without_an_allocation)
{
    uint8_t bytes[M_SIZE] = {};
    STORAGE storage;
    const ALLOCATOR allocator = M_Init(&storage);
    for (size_t prefix = 0; prefix < M_SIZE; prefix++) {
        STAGE_DATA data = { .stages = (const STAGE_DATA_STAGE *)bytes };
        CHECK(!Result_Ignore(M_Load(bytes, prefix, &allocator, &data)));
        CHECK(data.stages == nullptr);
        CHECK_EQ_INT(storage.allocations, 0);
        CHECK_EQ_INT(storage.arena.used, 0);
    }
    READER failed;
    Reader_OpenBuffer(&failed, bytes, sizeof(bytes), nullptr);
    Reader_Seek(&failed, sizeof(bytes) + 1);
    STAGE_DATA data;
    CHECK(!Result_Ignore(StageData_Load(&failed, &allocator, &data)));
    CHECK(data.stages == nullptr);
    CHECK_EQ_INT(storage.allocations, 0);
    Arena_Free(&storage.arena);
}

TEST(layouts_share_an_arena_and_release_together)
{
    uint8_t bytes[M_SIZE] = {};
    STORAGE storage;
    const ALLOCATOR allocator = M_Init(&storage);
    STAGE_DATA first;
    STAGE_DATA second;
    bytes[22] = 8;
    CHECK(Result_Ignore(M_Load(bytes, sizeof(bytes), &allocator, &first)));
    bytes[22] = 1;
    CHECK(Result_Ignore(M_Load(bytes, sizeof(bytes), &allocator, &second)));
    CHECK(first.stages != second.stages);
    CHECK(first.stages[0].cells[0][0].card);
    CHECK(second.stages[0].cells[0][3].card);
    CHECK(!first.stages[0].cells[0][3].card);
    CHECK_EQ_INT(storage.allocations, 2);
    Arena_Reset(&storage.arena);
    CHECK_EQ_INT(storage.arena.used, 0);
    Arena_Free(&storage.arena);
}

// Windows prevents a file opened by READER from shrinking.
#ifndef _WIN32
TEST(a_late_file_read_failure_returns_no_layouts_and_allocates_nothing)
{
    const char *const path = "test_th01_stage_data_truncated.bin";
    uint8_t bytes[READER_BUFFER_SIZE + 100] = {};
    const size_t start = sizeof(bytes) - M_SIZE;
    FILE *file = fopen(path, "wb");
    CHECK(file != nullptr);
    if (file == nullptr) {
        return;
    }
    CHECK_EQ_INT(fwrite(bytes, 1, sizeof(bytes), file), sizeof(bytes));
    CHECK_EQ_INT(fclose(file), 0);
    READER reader;
    const bool opened =
        Result_Log(Reader_OpenFile(&reader, path), LOG_LEVEL_ERROR);
    CHECK(opened);
    if (!opened) {
        CHECK(Result_Ignore(File_Remove(path)));
        return;
    }
    Reader_ReadU8(&reader);
    Reader_Seek(&reader, start);
    file = fopen(path, "wb");
    CHECK(file != nullptr);
    if (file == nullptr) {
        Reader_Close(&reader);
        CHECK(Result_Ignore(File_Remove(path)));
        return;
    }
    CHECK_EQ_INT(fclose(file), 0);
    STORAGE storage;
    const ALLOCATOR allocator = M_Init(&storage);
    Arena_Alloc(&storage.arena, 16);
    const size_t used = storage.arena.used;
    STAGE_DATA data;
    CHECK(!Result_Ignore(StageData_Load(&reader, &allocator, &data)));
    CHECK(Reader_Failed(&reader));
    CHECK(data.stages == nullptr);
    CHECK_EQ_INT(storage.allocations, 0);
    CHECK_EQ_INT(storage.arena.used, used);
    Reader_Close(&reader);
    CHECK(Result_Ignore(File_Remove(path)));
    Arena_Free(&storage.arena);
}
#endif
