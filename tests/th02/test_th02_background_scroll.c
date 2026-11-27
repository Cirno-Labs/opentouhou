#include "tests/test.h"
#include "th02/background.h"
#include "th02/background/private.h"
#include "th02/grcg.h"
#include "th02/video.h"

#include "core/base/arena.h"

#include <string.h>

TEST(reset_and_map_end_keep_pending_rows_and_draw_parameters)
{
    memset(&p_Background->scroll, 0xa5, sizeof(p_Background->scroll));
    Background_ResetScroll();
    CHECK_EQ_INT(p_Background->scroll.map_row, 23);
    CHECK_EQ_INT(p_Background->scroll.line, 0);
    CHECK_EQ_INT(p_Background->scroll.copy_top, (int16_t)0xa5a5);
    CHECK_EQ_INT(p_Background->scroll.copy_height, (int16_t)0xa5a5);
    CHECK_EQ_INT(p_Background->scroll.row[23], 0xa5);
    memset(&p_Background->cache, 0x5a, sizeof(p_Background->cache));
    const TILE_CACHE before = p_Background->cache;
    memset(&p_Background->map, 0, sizeof(p_Background->map));
    p_Background->map.count = 3;
    CHECK(Background_Scroll(
        (PLAYFIELD_PAGES) { .back = 0, .front = 1, .scroll = 399 }, 1));
    CHECK_EQ_INT(p_Background->scroll.map_row, 24);
    CHECK_EQ_INT(p_Background->scroll.line, 15);
    CHECK_EQ_INT(p_Background->scroll.copy_top, (int16_t)0xa5a5);
    CHECK_EQ_INT(p_Background->scroll.copy_height, (int16_t)0xa5a5);
    CHECK_EQ_INT(p_Background->scroll.row[23], 0xa5);
    CHECK(
        memcmp(&p_Background->cache, &before, sizeof(p_Background->cache))
        == 0);
}

TEST(unsigned_map_wrap_and_signed_line_arithmetic_keep_dirty_bytes)
{
    ARENA arena;
    Arena_Init(&arena, "tile scroll wrap drawing", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    Grcg_Init();
    memset(p_Background->sources, 0, sizeof(p_Background->sources));
    MAP_SECTION sections[MAP_SECTION_COUNT] = {};
    sections[7].tiles[0][23] = 99;
    const MAP tiles = { sections };
    const uint8_t order[] = { 7 };
    memset(&p_Background->map, 0, sizeof(p_Background->map));
    p_Background->map.map = &tiles;
    p_Background->map.order = order;
    p_Background->map.count = 1;
    memset(&p_Background->scroll, 0, sizeof(p_Background->scroll));
    p_Background->scroll.map_row = UINT16_MAX;
    p_Background->scroll.line = 0;
    memset(&p_Background->cache, 0xa5, sizeof(p_Background->cache));
    CHECK(!Background_Scroll(
        (PLAYFIELD_PAGES) { .back = 2, .front = 3, .scroll = 16 }, 1));
    CHECK_EQ_INT(p_Background->scroll.map_row, 0);
    CHECK_EQ_INT(p_Background->scroll.line, 15);
    CHECK_EQ_INT(p_Background->scroll.copy_top, 15);
    CHECK_EQ_INT(p_Background->scroll.copy_height, 1);
    CHECK_EQ_INT(p_Background->scroll.row[23], 99);
    CHECK_EQ_INT(p_Background->cache.images[1][23], 99);
    CHECK_EQ_INT(p_Background->cache.images[0][23], 0xa5);
    CHECK_EQ_INT(p_Background->cache.mode, 0xa5);
    CHECK_EQ_INT(p_Background->cache.columns[0], 0xa5);
    CHECK_EQ_INT(p_Background->cache.dirty[1][0], 0xa5);
    CHECK(!Background_Scroll(
        (PLAYFIELD_PAGES) { .back = 2, .front = 3, .scroll = 15 }, 1));
    CHECK_EQ_INT(p_Background->scroll.line, 14);
    CHECK_EQ_INT(p_Background->scroll.map_row, 0);
    CHECK_EQ_INT(p_Background->scroll.copy_top, 14);
    CHECK_EQ_INT(p_Background->cache.images[0][23], 0xa5);
    CHECK_EQ_INT(g_Video.drawing_page, 0);
    Arena_Free(&arena);
}

TEST(scroll_draws_partial_strips_on_both_pages_and_the_extra_back_column)
{
    ARENA arena;
    Arena_Init(&arena, "TH02 tile scrolling test", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    for (size_t page = 0; page < 2; page++) {
        for (size_t row = 0; row < 16; row++) {
            memset(g_Video.pages[page] + (72 + row * 80) * 8, row, 16);
        }
    }
    Grcg_Init();
    for (size_t i = 0; i < TILE_SOURCE_COUNT; i++) {
        p_Background->sources[i] = 72;
    }
    MAP_SECTION sections[MAP_SECTION_COUNT] = {};
    memset(sections[3].tiles[0], 5, MAP_COLUMNS);
    const MAP tiles = { sections };
    const uint8_t order[] = { 0, 1, 2, 3 };
    memset(&p_Background->map, 0, sizeof(p_Background->map));
    p_Background->map.map = &tiles;
    p_Background->map.order = order;
    p_Background->map.count = 4;
    memset(&p_Background->scroll, 0, sizeof(p_Background->scroll));
    Background_ResetScroll();
    memset(&p_Background->cache, 0, sizeof(p_Background->cache));
    p_Background->cache.mode = TILE_IMAGES;
    CHECK(!Background_Scroll(
        (PLAYFIELD_PAGES) { .back = 0, .front = 1, .scroll = 398 }, 2));
    CHECK_EQ_INT(p_Background->scroll.map_row, 24);
    CHECK_EQ_INT(p_Background->scroll.line, 14);
    CHECK_EQ_INT(p_Background->scroll.copy_top, 14);
    CHECK_EQ_INT(p_Background->scroll.copy_height, 2);
    CHECK_EQ_INT(p_Background->cache.images[24][23], 5);
    CHECK_EQ_INT(g_Video.drawing_page, 0);
    for (size_t page = 0; page < 2; page++) {
        for (size_t y = 398; y < 400; y++) {
            const size_t left = page == 0 ? 16 : 32;
            CHECK_EQ_INT(g_Video.pages[page][y * 640 + left - 1], 0);
            for (size_t x = left; x < 416; x++) {
                CHECK_EQ_INT(g_Video.pages[page][y * 640 + x], y - 384);
            }
            CHECK_EQ_INT(g_Video.pages[page][y * 640 + 416], 0);
        }
    }
    Arena_Free(&arena);
}
