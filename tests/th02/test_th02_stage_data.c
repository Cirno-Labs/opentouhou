#include "tests/test.h"
#include "th02/stage/data.h"

#include "core/base/arena.h"
#include "core/io/reader.h"

#include <string.h>

#define M_SIZE 176

static void M_U16(uint8_t *const bytes, const uint16_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
}

static void M_U32(uint8_t *const bytes, const uint32_t value)
{
    M_U16(bytes, (uint16_t)value);
    M_U16(bytes + 2, (uint16_t)(value >> 16));
}

static void M_File(uint8_t bytes[M_SIZE])
{
    memset(bytes, 0, M_SIZE);
    M_U32(bytes, M_SIZE - 4);
    M_U16(bytes + 4, 3);
    bytes[6] = 2;
    bytes[7] = 1;
    bytes[8] = 2;
    M_U16(bytes + 9, 2);
    M_U16(bytes + 11, (uint16_t)-2);
    M_U16(bytes + 13, (uint16_t)-16);
    M_U16(bytes + 15, (uint16_t)-4);
    M_U16(bytes + 17, 9);
    M_U16(bytes + 19, 17);
    M_U16(bytes + 21, 3);
    M_U16(bytes + 23, 5);
    M_U16(bytes + 25, 7);
    bytes[27] = 6;
    bytes[28] = (uint8_t)-7;
    M_U32(bytes + 29, (uint32_t)-123456);
    M_U16(bytes + 33, (uint16_t)-2);
    bytes[35] = 0x80;
    bytes[36] = (uint8_t)-3;
    M_U16(bytes + 37, (uint16_t)-3);
    bytes[39] = 1;
    bytes[40] = 2;
    bytes[41] = 3;
    bytes[42] = 0xff;
    memcpy(bytes + 43, bytes + 11, 28);
    bytes[71] = 0xfe;
    bytes[72] = 0;
    bytes[73] = 0x66;
    M_U16(bytes + 74, 2);
    M_U16(bytes + 76, 12);
    memset(bytes + 78, 0xff, 48);
    bytes[78] = 0;
    bytes[125] = 1;
    M_U16(bytes + 126, 5);
    memset(bytes + 128, 0xff, 48);
    bytes[143] = 1;
}

static RESULT M_Load(
    const void *const bytes, const size_t size, ARENA *const arena,
    STAGE_DATA *const data)
{
    READER reader;
    Reader_OpenBuffer(&reader, bytes, size, nullptr);
    const RESULT result = StageData_Load(&reader, Arena_Allocator(arena), data);
    Reader_Close(&reader);
    return result;
}

TEST(loads_signed_formations_shared_scripts_and_unsorted_schedule_rows)
{
    uint8_t bytes[M_SIZE];
    M_File(bytes);
    ARENA arena;
    Arena_Init(&arena, "stage data test", 32768);
    STAGE_DATA data;
    const bool loaded = Result_Log(
        M_Load(bytes, sizeof(bytes), &arena, &data), LOG_LEVEL_ERROR);
    CHECK(loaded);
    if (loaded) {
        CHECK_EQ_INT(data.map_count, 3);
        CHECK_EQ_INT(data.map[0], 2);
        CHECK_EQ_INT(data.map[1], 1);
        CHECK_EQ_INT(data.map[2], 2);
        CHECK_EQ_INT(data.formation_count, 2);
        const STAGE_FORMATION *const formation = &data.formations[0];
        CHECK_EQ_INT(formation->reserved_00, -2);
        CHECK_EQ_INT(formation->spawn_top, -16);
        CHECK_EQ_INT(formation->bullet_offset_x, -4);
        CHECK_EQ_INT(formation->bullet_offset_y, 9);
        CHECK_EQ_INT(formation->pattern, 17);
        CHECK_EQ_INT(formation->image_count, 3);
        CHECK_EQ_INT(formation->frames_per_image, 5);
        CHECK_EQ_INT(formation->kill_pattern_offset, 7);
        CHECK_EQ_INT(formation->item, 6);
        CHECK_EQ_INT(formation->reserved_11, -7);
        CHECK_EQ_INT(formation->score, -123456);
        CHECK_EQ_INT(formation->hp, -2);
        CHECK_EQ_INT(formation->player_collision_disabled, 0x80);
        CHECK_EQ_INT(formation->reserved_19, -3);
        CHECK_EQ_INT(formation->aimed_interval, -3);
        CHECK_EQ_INT(data.script_count, 1);
        CHECK_EQ_INT(data.formations[0].script, 0);
        CHECK_EQ_INT(data.formations[1].script, 0);
        CHECK_EQ_INT(data.scripts[0].size, 3);
        CHECK(memcmp(data.scripts[0].bytes, "\x01\x02\x03\x00", 4) == 0);
        CHECK_EQ_INT(data.declared_line_count, 2);
        CHECK_EQ_INT(data.line_count, 2);
        CHECK_EQ_INT(data.lines[0].position, 12);
        CHECK_EQ_INT(data.lines[1].position, 5);
        CHECK_EQ_INT(data.lines[0].formations[0], 0);
        CHECK_EQ_INT(data.lines[0].formations[1], -1);
        CHECK_EQ_INT(data.lines[0].formations[47], 1);
        CHECK_EQ_INT(data.lines[1].formations[15], 1);
    }
    memset(bytes, 0, sizeof(bytes));
    if (loaded) {
        CHECK_EQ_INT(data.formations[1].score, -123456);
        CHECK_EQ_INT(data.lines[1].position, 5);
    }
    Arena_Free(&arena);
}

TEST(uses_the_dos_length_and_leaves_outer_trailing_bytes_unread)
{
    uint8_t bytes[M_SIZE + 1];
    M_File(bytes);
    M_U32(bytes, 0x10000 + M_SIZE - 4);
    bytes[M_SIZE] = 0x5a;
    READER reader;
    Reader_OpenBuffer(&reader, bytes, sizeof(bytes), nullptr);
    ARENA arena;
    Arena_Init(&arena, "stage data test", 32768);
    STAGE_DATA data;
    CHECK(Result_Log(
        StageData_Load(&reader, Arena_Allocator(&arena), &data),
        LOG_LEVEL_ERROR));
    CHECK_EQ_INT(Reader_Tell(&reader), M_SIZE);
    CHECK_EQ_INT(Reader_ReadU8(&reader), 0x5a);
    Reader_Close(&reader);
    Arena_Free(&arena);
}

TEST(reports_missing_whole_schedule_rows_without_inventing_contents)
{
    uint8_t bytes[M_SIZE];
    M_File(bytes);
    M_U16(bytes + 74, 6);
    ARENA arena;
    Arena_Init(&arena, "stage data test", 32768);
    STAGE_DATA data;
    const bool loaded = Result_Log(
        M_Load(bytes, sizeof(bytes), &arena, &data), LOG_LEVEL_ERROR);
    CHECK(loaded);
    if (loaded) {
        CHECK_EQ_INT(data.declared_line_count, 6);
        CHECK_EQ_INT(data.line_count, 2);
        CHECK_EQ_INT(data.lines[1].position, 5);
    }
    Arena_Free(&arena);
}

TEST(rejects_all_truncated_file_prefixes_without_arena_allocations)
{
    uint8_t bytes[M_SIZE];
    M_File(bytes);
    ARENA arena;
    Arena_Init(&arena, "stage data test", 32768);
    for (size_t size = 0; size < sizeof(bytes); size++) {
        STAGE_DATA data;
        CHECK(!Result_Ignore(M_Load(bytes, size, &arena, &data)));
        CHECK(data.map == nullptr && data.formations == nullptr);
        CHECK(data.scripts == nullptr && data.lines == nullptr);
        CHECK_EQ_INT(data.line_count, 0);
        CHECK_EQ_INT(arena.used, 0);
    }
    Arena_Free(&arena);
}

TEST(rejects_invalid_counts_indices_references_and_partial_rows)
{
    const size_t offsets[] = { 4, 4, 6, 9, 9, 72, 74, 78, 0 };
    const uint16_t values[] = { 321, 0xffff, 16, 129,       0xffff,
                                1,   0xffff, 2,  M_SIZE - 5 };
    ARENA arena;
    Arena_Init(&arena, "stage data test", 32768);
    for (size_t i = 0; i < sizeof(offsets) / sizeof(offsets[0]); i++) {
        uint8_t bytes[M_SIZE];
        M_File(bytes);
        if (offsets[i] == 6 || offsets[i] == 72 || offsets[i] == 78) {
            bytes[offsets[i]] = (uint8_t)values[i];
        } else if (offsets[i] == 0) {
            M_U32(bytes, values[i]);
        } else {
            M_U16(bytes + offsets[i], values[i]);
        }
        STAGE_DATA data;
        CHECK(!Result_Ignore(M_Load(bytes, sizeof(bytes), &arena, &data)));
        CHECK(data.lines == nullptr);
        CHECK_EQ_INT(arena.used, 0);
    }
    Arena_Free(&arena);
}

TEST(accepts_64_script_bytes_and_rejects_longer_or_unterminated_scripts)
{
    ARENA arena;
    Arena_Init(&arena, "stage data test", 32768);
    for (size_t length = 64; length <= 65; length++) {
        uint8_t bytes[128] = {};
        const size_t size = 4 + 2 + 2 + 28 + length + 1 + 2;
        M_U32(bytes, (uint32_t)size - 4);
        M_U16(bytes + 6, 1);
        memset(bytes + 36, 1, length);
        bytes[36 + length] = 0xff;
        STAGE_DATA data;
        const bool loaded = Result_Ignore(M_Load(bytes, size, &arena, &data));
        CHECK(loaded == (length == 64));
        if (loaded) {
            CHECK_EQ_INT(data.scripts[0].size, 64);
            CHECK_EQ_INT(data.scripts[0].bytes[63], 1);
        } else {
            CHECK_EQ_INT(arena.used, 0);
        }
        Arena_Reset(&arena);
        bytes[36 + length] = 1;
        CHECK(!Result_Ignore(M_Load(bytes, size, &arena, &data)));
        CHECK_EQ_INT(arena.used, 0);
    }
    Arena_Free(&arena);
}

TEST(enforces_the_inline_script_table_limit)
{
    uint8_t bytes[2048] = {};
    M_U16(bytes + 6, 41);
    size_t position = 8;
    for (size_t i = 0; i < 41; i++) {
        position += 28;
        bytes[position++] = 0;
        bytes[position++] = 0xff;
    }
    position += 2;
    M_U32(bytes, (uint32_t)position - 4);
    ARENA arena;
    Arena_Init(&arena, "stage data test", 32768);
    STAGE_DATA data;
    CHECK(!Result_Ignore(M_Load(bytes, position, &arena, &data)));
    CHECK_EQ_INT(arena.used, 0);
    Arena_Free(&arena);
}

TEST(empty_tables_allocate_nothing_from_the_asset_arena)
{
    uint8_t bytes[10] = { 6 };
    ARENA arena;
    Arena_Init(&arena, "stage data test", 32768);
    STAGE_DATA data;
    CHECK(Result_Log(
        M_Load(bytes, sizeof(bytes), &arena, &data), LOG_LEVEL_ERROR));
    CHECK(data.map == nullptr && data.formations == nullptr);
    CHECK(data.scripts == nullptr && data.lines == nullptr);
    CHECK_EQ_INT(data.line_count, 0);
    CHECK_EQ_INT(data.declared_line_count, 0);
    CHECK_EQ_INT(arena.used, 0);
    Arena_Free(&arena);
}

TEST(rank_adjustments_preserve_signed_shift_and_wrap)
{
    STAGE_FORMATION formation = { .aimed_interval = -3 };
    CHECK_EQ_INT(StageData_AimedInterval(&formation, 0), -6);
    CHECK_EQ_INT(StageData_AimedInterval(&formation, 1), -3);
    CHECK_EQ_INT(StageData_AimedInterval(&formation, 2), -2);
    CHECK_EQ_INT(StageData_AimedInterval(&formation, 3), -2);
    CHECK_EQ_INT(StageData_AimedInterval(&formation, 255), -3);
    formation.aimed_interval = INT16_MAX;
    CHECK_EQ_INT(StageData_AimedInterval(&formation, 0), -2);
    formation.aimed_interval = INT16_MIN;
    CHECK_EQ_INT(StageData_AimedInterval(&formation, 0), 0);
    CHECK_EQ_INT(StageData_AimedInterval(&formation, 2), -16384);
    formation.aimed_interval = INT16_MIN + 1;
    CHECK_EQ_INT(StageData_AimedInterval(&formation, 0), 2);
    formation.aimed_interval = 3;
    CHECK_EQ_INT(StageData_AimedInterval(&formation, 3), 1);
}
