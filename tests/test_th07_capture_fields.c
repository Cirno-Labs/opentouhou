#include "../replay/th07/fields.h"
#include "case.h"

#include "core/base/arena.h"
#include "core/base/memory.h"
#include "core/base/utils.h"
#include "test.h"

#include <string.h>

#define M_ARENA_SIZE (1024 * 1024)

// Fails to compile for a column kind that the cases do not know.
enum { M_KIND_U32, M_KIND_I32, M_KIND_F32, M_KIND_HASH };
#define M_CHECK_KIND(name, kind) static_assert(M_KIND_##kind >= 0);
TH07_START_FIELDS(M_CHECK_KIND)
TH07_CALC_FIELDS(M_CHECK_KIND)
#undef M_CHECK_KIND

static const char *const M_START_NAMES[] = { TH07_START_NAMES };
static const char *const M_CALC_NAMES[] = { TH07_CALC_NAMES };

static_assert(ARRAY_SIZE(M_START_NAMES) == TH07_START_COUNT);
static_assert(ARRAY_SIZE(M_CALC_NAMES) == TH07_CALC_COUNT);

static bool M_NamesAreUnique(const char *const *names, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        for (size_t j = i + 1; j < count; j++) {
            if (strcmp(names[i], names[j]) == 0) {
                return false;
            }
        }
    }
    return true;
}

TEST(each_table_names_a_column_once)
{
    CHECK(M_NamesAreUnique(M_START_NAMES, ARRAY_SIZE(M_START_NAMES)));
    CHECK(M_NamesAreUnique(M_CALC_NAMES, ARRAY_SIZE(M_CALC_NAMES)));
}

TEST(a_th07_case_round_trips_with_its_columns)
{
    ARENA arena;
    Arena_Init(&arena, "test", M_ARENA_SIZE);
    uint32_t start[TH07_START_COUNT];
    for (uint32_t i = 0; i < TH07_START_COUNT; i++) {
        start[i] = 100 + i;
    }
    const CASE_HEADER header = {
        .game_id = "th07",
        .info = "th07 1.00b",
        .start_names = { .items = (const char **)M_START_NAMES,
                         .count = TH07_START_COUNT },
        .start = { .items = start, .count = TH07_START_COUNT },
        .calc_names = { .items = (const char **)M_CALC_NAMES,
                        .count = TH07_CALC_COUNT },
    };
    uint32_t row[TH07_CALC_COUNT];
    for (uint32_t i = 0; i < TH07_CALC_COUNT; i++) {
        row[i] = i * 0x01010101u;
    }

    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator, nullptr);
    CASE_WRITER *const case_writer = Case_OpenWriter(&writer, &header);
    Case_WriteRow(case_writer, row);
    CHECK(Result_Log(Case_CloseWriter(case_writer), LOG_LEVEL_ERROR));
    size_t size;
    uint8_t *const data = Writer_TakeBuffer(&writer, &size);
    Result_Ignore(Writer_Close(&writer));

    READER reader;
    Reader_OpenBuffer(&reader, data, size, nullptr);
    CASE_READER *case_reader;
    CHECK(Result_Log(
        Case_OpenReader(&reader, &arena, &case_reader), LOG_LEVEL_ERROR));
    const CASE_HEADER *const read = Case_GetHeader(case_reader);
    CHECK_EQ_STR(read->game_id, "th07");
    CHECK_EQ_INT(read->start.count, TH07_START_COUNT);
    CHECK_EQ_INT(read->start.items[TH07_START_cherry_plus], 113);
    CHECK_EQ_INT(
        Case_FindColumn(&read->start_names, "rng_seed"), TH07_START_rng_seed);
    CHECK_EQ_INT(
        Case_FindColumn(&read->calc_names, "events"), TH07_CALC_events);
    CHECK_EQ_INT(
        Case_FindColumn(&read->calc_names, "item_hash"), TH07_CALC_item_hash);
    const uint32_t *const read_row = Case_ReadRow(case_reader);
    CHECK(read_row != nullptr);
    if (read_row != nullptr) {
        CHECK(memcmp(read_row, row, sizeof(row)) == 0);
    }
    CHECK(Case_ReadRow(case_reader) == nullptr);
    CHECK(Result_Log(Case_CloseReader(case_reader), LOG_LEVEL_ERROR));

    Mem_Free(data);
    Arena_Free(&arena);
}
