#include "tests/test.h"
#include "th02/map.h"

#include "core/base/arena.h"

#define M_FILE_SIZE (10 + MAP_SECTION_COUNT * MAP_SECTION_ROWS * MAP_COLUMNS)

static void M_File(uint8_t bytes[M_FILE_SIZE])
{
    memcpy(bytes, "MIKO_MAP  ", 10);
    for (size_t section = 0; section < MAP_SECTION_COUNT; section++) {
        for (size_t row = 0; row < MAP_SECTION_ROWS; row++) {
            for (size_t column = 0; column < MAP_COLUMNS; column++) {
                bytes[10 + (section * 8 + row) * 24 + column] =
                    (uint8_t)(section * 29 + row * 17 + column);
            }
        }
    }
}

TEST(loads_all_sections_in_row_order_and_preserves_all_tile_indices)
{
    uint8_t bytes[M_FILE_SIZE];
    M_File(bytes);
    READER reader;
    Reader_OpenBuffer(&reader, bytes, sizeof(bytes), nullptr);
    ARENA arena;
    Arena_Init(&arena, "map test", 8192);
    MAP map;
    const bool loaded = Result_Log(
        Map_Load(&reader, Arena_Allocator(&arena), &map), LOG_LEVEL_ERROR);
    CHECK(loaded);
    if (loaded) {
        for (size_t section = 0; section < MAP_SECTION_COUNT; section++) {
            for (size_t row = 0; row < MAP_SECTION_ROWS; row++) {
                for (size_t column = 0; column < MAP_COLUMNS; column++) {
                    CHECK_EQ_INT(
                        map.sections[section].tiles[row][column],
                        (uint8_t)(section * 29 + row * 17 + column));
                }
            }
        }
        CHECK_EQ_INT(Reader_Remaining(&reader), 0);
        CHECK_EQ_INT(arena.used, 3072);
    }
    Reader_Close(&reader);
    memset(bytes, 0, sizeof(bytes));
    if (loaded) {
        CHECK_EQ_INT(map.sections[1].tiles[0][0], 29);
    }
    Arena_Free(&arena);
}

TEST(skips_the_unchecked_header_at_the_current_position)
{
    uint8_t bytes[5 + M_FILE_SIZE + 1] = {};
    M_File(bytes + 5);
    memset(bytes + 5, 0xff, 10);
    bytes[sizeof(bytes) - 1] = 0x5a;
    READER reader;
    Reader_OpenBuffer(&reader, bytes, sizeof(bytes), nullptr);
    Reader_Skip(&reader, 5);
    ARENA arena;
    Arena_Init(&arena, "map test", 8192);
    MAP map;
    const bool loaded = Result_Log(
        Map_Load(&reader, Arena_Allocator(&arena), &map), LOG_LEVEL_ERROR);
    CHECK(loaded);
    if (loaded) {
        CHECK_EQ_INT(map.sections[0].tiles[0][1], 1);
        CHECK_EQ_INT(Reader_Tell(&reader), 5 + M_FILE_SIZE);
        CHECK_EQ_INT(Reader_ReadU8(&reader), 0x5a);
    }
    Reader_Close(&reader);
    Arena_Free(&arena);
}

TEST(rejects_every_truncated_prefix_without_arena_allocations)
{
    uint8_t bytes[M_FILE_SIZE];
    M_File(bytes);
    ARENA arena;
    Arena_Init(&arena, "map test", 8192);
    for (size_t size = 0; size < sizeof(bytes); size++) {
        READER reader;
        Reader_OpenBuffer(&reader, bytes, size, nullptr);
        MAP map;
        CHECK(!Result_Ignore(Map_Load(&reader, Arena_Allocator(&arena), &map)));
        CHECK(map.sections == nullptr);
        CHECK_EQ_INT(arena.used, 0);
        Reader_Close(&reader);
    }
    Arena_Free(&arena);
}

TEST(read_failures_leave_the_output_empty)
{
    uint8_t bytes[M_FILE_SIZE];
    M_File(bytes);
    READER reader;
    Reader_OpenBuffer(&reader, bytes, sizeof(bytes), nullptr);
    Reader_Seek(&reader, sizeof(bytes) + 1);
    ARENA arena;
    Arena_Init(&arena, "map test", 8192);
    MAP_SECTION old_section = {};
    MAP map = { .sections = &old_section };
    CHECK(!Result_Ignore(Map_Load(&reader, Arena_Allocator(&arena), &map)));
    CHECK(map.sections == nullptr);
    CHECK_EQ_INT(arena.used, 0);
    Reader_Close(&reader);
    Arena_Free(&arena);
}
