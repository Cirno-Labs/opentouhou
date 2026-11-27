#include "tests/test.h"
#include "tests/th02/th02_clip_fixture.h"
#include "th02/background.h"
#include "th02/background/private.h"
#include "th02/grcg.h"
#include "th02/video.h"

#include "core/base/arena.h"

TEST(source_strip_initial_rows_and_full_redraw_keep_the_unfilled_last_row)
{
    ARENA arena;
    Arena_Init(&arena, "TH02 tile setup test", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    memset(g_Video.pages[0], 11, VIDEO_PAGE_SIZE);
    memset(g_Video.pages[1], 12, VIDEO_PAGE_SIZE);
    Video_ShowPage(1);
    Video_SetSplit(123, 45, 67);
    Grcg_Init();
    uint8_t pixels[TILE_SOURCE_COUNT * 256];
    for (size_t image = 0; image < TILE_SOURCE_COUNT; image++) {
        memset(pixels + image * 256, image % 16, 256);
    }
    for (size_t image = 0; image < TILE_SOURCE_COUNT; image++) {
        p_Background->sources[image] = 0xdead;
    }
    const MPN images = { .count = 100, .pixels = pixels };
    Background_SetSources(&images);
    CHECK_EQ_INT(g_Video.drawing_page, 0);
    CHECK_EQ_INT(p_Background->sources[0], 72);
    CHECK_EQ_INT(p_Background->sources[99], 30798);
    for (size_t page = 0; page < 2; page++) {
        for (size_t image = 0; image < TILE_SOURCE_COUNT; image++) {
            for (size_t row = 0; row < 16; row++) {
                for (size_t x = 0; x < 16; x++) {
                    CHECK_EQ_INT(
                        g_Video
                            .pages[page]
                                  [(p_Background->sources[image] + row * 80) * 8
                                   + x],
                        image % 16);
                }
            }
        }
    }
    MAP_SECTION sections[MAP_SECTION_COUNT] = {};
    for (size_t y = 0; y < 24; y++) {
        for (size_t x = 0; x < TILE_COLUMNS; x++) {
            sections[y / 8].tiles[y % 8][x] = (y * 24 + x) % 100;
        }
    }
    const MAP tiles = { sections };
    const uint8_t order[] = { 0, 1, 2 };
    memset(&p_Background->map, 0, sizeof(p_Background->map));
    p_Background->map.map = &tiles;
    p_Background->map.order = order;
    p_Background->map.count = 3;
    memset(&p_Background->cache, 0xa5, sizeof(p_Background->cache));
    p_Background->cache.mode = TILE_IMAGES;
    memset(p_Background->cache.images[24], 99, TILE_COLUMNS);
    Video_AccessPage(1);
    Background_DrawInitial(&images);
    CHECK_EQ_INT(p_Background->cache.images[23][0], 0);
    CHECK_EQ_INT(p_Background->cache.images[0][0], 52);
    CHECK_EQ_INT(p_Background->cache.images[24][0], 99);
    CHECK_EQ_INT(p_Background->cache.columns[0], 0xa5);
    CHECK_EQ_INT(p_Background->cache.dirty[0][0], 0xa5);
    CHECK_EQ_INT(g_Video.pages[1][368 * 640 + 32], 0);
    CHECK_EQ_INT(g_Video.pages[1][32], 4);
    CHECK_EQ_INT(g_Video.pages[1][384 * 640 + 32], 12);
    CHECK_EQ_INT(g_Video.pages[0][32], 11);
    Video_AccessPage(0);
    TestClip_Set(600, 200, 607, 201);
    memset(&p_Background->scroll, 0, sizeof(p_Background->scroll));
    p_Background->scroll.map_row = 42;
    p_Background->scroll.line = 7;
    p_Background->scroll.row[0] = 9;
    const TILE_CACHE before = p_Background->cache;
    Background_RepaintAll();
    CHECK(
        memcmp(&p_Background->cache, &before, sizeof(p_Background->cache))
        == 0);
    CHECK_EQ_INT(g_Video.pages[0][32], 4);
    CHECK_EQ_INT(g_Video.pages[0][368 * 640 + 32], 0);
    CHECK_EQ_INT(g_Video.pages[0][384 * 640 + 32], 3);
    CHECK_EQ_INT(g_Video.pages[0][384 * 640 + 416], 11);
    CHECK_EQ_INT(g_Video.pages[1][384 * 640 + 32], 12);
    CHECK_EQ_INT(p_Background->scroll.map_row, 42);
    CHECK_EQ_INT(p_Background->scroll.line, 7);
    CHECK_EQ_INT(p_Background->scroll.row[0], 9);
    CHECK_EQ_INT(p_Background->scroll.copy_top, 0);
    CHECK_EQ_INT(p_Background->scroll.copy_height, 16);
    CHECK_EQ_INT(g_Video.drawing_page, 0);
    CHECK_EQ_INT(g_Video.visible_page, 1);
    CHECK_EQ_INT(g_Video.first_lines, 123);
    CHECK_EQ_INT(g_Video.first_address, 45);
    CHECK_EQ_INT(g_Video.second_address, 67);
    CHECK_EQ_INT(g_Grcg->mode, 0);
    Arena_Free(&arena);
}

TEST(full_redraw_in_disabled_mode_blacks_an_inclusive_byte_rectangle)
{
    ARENA arena;
    Arena_Init(&arena, "TH02 full black redraw test", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    memset(g_Video.pages[0], 15, VIDEO_PAGE_SIZE);
    memset(g_Video.pages[1], 7, VIDEO_PAGE_SIZE);

    Grcg_SetColor(5);
    Grcg_SetPlanes(1);
    TestClip_Set(600, 20, 607, 25);
    memset(&p_Background->cache, 0, sizeof(p_Background->cache));
    p_Background->cache.mode = TILE_DISABLED;
    p_Background->cache.redraw_all = 1;
    const TILE_CACHE before = p_Background->cache;
    memset(&p_Background->scroll, 0, sizeof(p_Background->scroll));
    Background_RepaintAll();
    CHECK(
        memcmp(&p_Background->cache, &before, sizeof(p_Background->cache))
        == 0);
    for (size_t pixel = 0; pixel < VIDEO_PAGE_SIZE; pixel++) {
        const size_t x = pixel % VIDEO_WIDTH;
        const size_t y = pixel / VIDEO_WIDTH;
        CHECK_EQ_INT(
            g_Video.pages[0][pixel],
            x >= 32 && x <= 423 && y >= 20 && y <= 25 ? 0 : 15);
        CHECK_EQ_INT(g_Video.pages[1][pixel], 7);
    }
    CHECK_EQ_INT(g_Grcg->mode, 0);
    for (size_t plane = 0; plane < 4; plane++) {
        CHECK_EQ_INT(g_Grcg->tiles[plane], 0);
    }
    Arena_Free(&arena);
}

TEST(source_strip_uses_only_loaded_tiles_and_blanks_unloaded_slots)
{
    ARENA arena;
    Arena_Init(&arena, "TH02 bounded tile sources", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    const uint16_t counts[] = { 0, 1, 27, 99, 100, 101 };
    Grcg_Init();
    for (size_t c = 0; c < ARRAY_SIZE(counts); c++) {
        const uint16_t count = counts[c];
        uint8_t *const pixels = count == 0 ? nullptr : Mem_Alloc(count * 256);
        if (pixels != nullptr) {
            memset(pixels, 3, count * 256);
        }
        const MPN tiles = { .count = count, .pixels = pixels };
        memset(g_Video.pages[0], 7, VIDEO_PAGE_SIZE);
        memset(g_Video.pages[1], 9, VIDEO_PAGE_SIZE);
        Video_AccessPage(1);
        Background_SetSources(&tiles);
        CHECK_EQ_INT(g_Video.drawing_page, 0);
        CHECK_EQ_INT(p_Background->last_source, (uint8_t)(count - 1));
        for (size_t page = 0; page < 2; page++) {
            CHECK_EQ_INT(g_Video.pages[page][575], page == 0 ? 7 : 9);
            for (size_t image = 0; image < TILE_SOURCE_COUNT; image++) {
                const uint8_t expected = image < count ? 3 : 0;
                for (size_t row = 0; row < 16; row++) {
                    for (size_t x = 0; x < 16; x++) {
                        CHECK_EQ_INT(
                            g_Video
                                .pages[page]
                                      [(p_Background->sources[image] + row * 80)
                                           * 8
                                       + x],
                            expected);
                    }
                }
            }
        }
        Mem_Free(pixels);
    }
    Arena_Free(&arena);
}
