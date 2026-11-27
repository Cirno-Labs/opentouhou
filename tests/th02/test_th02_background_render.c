#include "tests/test.h"
#include "th02/background.h"
#include "th02/background/private.h"
#include "th02/grcg_private.h"
#include "th02/video.h"

#include "core/base/arena.h"

#include <string.h>

TEST(dirty_tile_copy_uses_raw_image_id_and_current_page)
{
    ARENA arena;
    Arena_Init(&arena, "TH02 tile render test", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    memset(g_Video.pages[0], 3, VIDEO_PAGE_SIZE);
    memset(g_Video.pages[1], 12, VIDEO_PAGE_SIZE);
    Video_AccessPage(1);
    Video_ShowPage(1);
    Video_SetSplit(123, 45, 67);
    Grcg_Init();
    memset(p_Background->sources, 0, sizeof(p_Background->sources));
    for (size_t row = 0; row < 16; row++) {
        memset(g_Video.pages[1] + (72 + row * 80) * 8, row | 0xa0, 16);
    }
    memset(&p_Background->cache, 0, sizeof(p_Background->cache));
    p_Background->cache.mode = TILE_IMAGES;
    TILE_CACHE *const cache = &p_Background->cache;
    cache->images[24][23] = 255;
    cache->images[11][17] = 72;
    memset(&p_Background->scroll, 0, sizeof(p_Background->scroll));
    cache->columns[23] = cache->dirty[23][24] = 1;
    Background_RepaintDirty();
    for (size_t pixel = 0; pixel < VIDEO_PAGE_SIZE; pixel++) {
        CHECK_EQ_INT(g_Video.pages[0][pixel], 3);
    }
    for (size_t row = 0; row < 16; row++) {
        const size_t first = (30770 + row * 80) * 8;
        CHECK_EQ_INT(g_Video.pages[1][first - 1], 12);
        for (size_t pixel = 0; pixel < 16; pixel++) {
            CHECK_EQ_INT(g_Video.pages[1][first + pixel], row | 0xa0);
        }
        CHECK_EQ_INT(g_Video.pages[1][first + 16], 12);
    }
    CHECK_EQ_INT(g_Video.pages[1][VIDEO_VISIBLE_SIZE], 12);
    CHECK_EQ_INT(g_Video.drawing_page, 1);
    CHECK_EQ_INT(g_Video.visible_page, 1);
    CHECK_EQ_INT(g_Video.first_lines, 123);
    CHECK_EQ_INT(g_Video.first_address, 45);
    CHECK_EQ_INT(g_Video.second_address, 67);
    CHECK_EQ_INT(p_Grcg->mode, 0);
    CHECK_EQ_INT(cache->columns[23], 0);
    Arena_Free(&arena);
}

TEST(overlapping_rows_read_the_result_of_the_previous_row)
{
    ARENA arena;
    Arena_Init(&arena, "TH02 overlapping tiles test", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    for (size_t row = 0; row < 17; row++) {
        memset(g_Video.pages[0] + row * VIDEO_WIDTH, row & 15, VIDEO_WIDTH);
    }
    Grcg_Init();
    memset(p_Background->sources, 0, sizeof(p_Background->sources));
    Render_CopyLines(80, 0, 0, 16, TILE_COPY_ALL);
    for (size_t row = 0; row < 17; row++) {
        for (size_t pixel = 0; pixel < 16; pixel++) {
            CHECK_EQ_INT(g_Video.pages[0][row * VIDEO_WIDTH + pixel], 0);
        }
        CHECK_EQ_INT(g_Video.pages[0][row * VIDEO_WIDTH + 16], row & 15);
    }
    Arena_Free(&arena);
}

TEST(black_and_disabled_passes_keep_charger_and_page_side_effects)
{
    ARENA arena;
    Arena_Init(&arena, "TH02 black tiles test", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    memset(g_Video.pages[0], 15, VIDEO_PAGE_SIZE);
    memset(g_Video.pages[1], 7, VIDEO_PAGE_SIZE);

    Grcg_SetColor(11);
    Grcg_SetPlanes(1);
    memset(&p_Background->cache, 0, sizeof(p_Background->cache));
    p_Background->cache.mode = TILE_BLACK;
    p_Background->cache.columns[0] = p_Background->cache.dirty[0][1] = 1;
    Background_RepaintDirty();
    for (size_t pixel = 0; pixel < VIDEO_PAGE_SIZE; pixel++) {
        const size_t x = pixel % VIDEO_WIDTH;
        const size_t y = pixel / VIDEO_WIDTH;
        CHECK_EQ_INT(
            g_Video.pages[0][pixel],
            x >= 32 && x < 48 && y >= 16 && y < 32 ? 0 : 15);
        CHECK_EQ_INT(g_Video.pages[1][pixel], 7);
    }
    CHECK_EQ_INT(p_Grcg->mode, 0);
    for (size_t plane = 0; plane < 4; plane++) {
        CHECK_EQ_INT(p_Grcg->tiles[plane], 0);
    }
    Grcg_SetColor(5);
    p_Grcg->mode = 0xc3;
    p_Background->cache.mode = TILE_DISABLED;
    p_Background->cache.redraw_all = 1;
    Background_RepaintDirty();
    CHECK_EQ_INT(p_Grcg->mode, 0xc3);
    CHECK_EQ_INT(p_Grcg->tiles[0], 255);
    CHECK_EQ_INT(p_Grcg->tiles[2], 255);
    CHECK_EQ_INT(p_Background->cache.redraw_all, 0);
    p_Background->cache.mode = TILE_BLACK;
    Background_RepaintDirty();
    CHECK_EQ_INT(p_Grcg->mode, 0);
    CHECK_EQ_INT(p_Grcg->tiles[0], 0);
    Arena_Free(&arena);
}

TEST(word_copy_uses_byte_lanes_when_only_one_address_is_odd)
{
    ARENA arena;
    Arena_Init(&arena, "tile byte lanes", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    Grcg_Init();
    memset(p_Background->sources, 0, sizeof(p_Background->sources));
    for (size_t source = 0; source < 2; source++) {
        for (size_t destination = 16; destination < 18; destination++) {
            for (size_t x = 0; x < 256; x++) {
                g_Video.pages[0][x] = (x / 8) & 15;
            }
            p_Background->sources[0] = source;
            Render_CopyLines(destination, 0, 0, 1, TILE_COPY_ALL);
            const size_t swap = ((source ^ destination) & 1) * 8;
            for (size_t x = 0; x < 16; x++) {
                CHECK_EQ_INT(
                    g_Video.pages[0][destination * 8 + x],
                    (source * 8 + (x ^ swap)) / 8);
            }
        }
    }
    Arena_Free(&arena);
}

TEST(set_both_updates_one_ring_entry_and_preserves_page_specific_sources)
{
    ARENA arena;
    Arena_Init(&arena, "tile ring writes", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));

    Grcg_SetColor(9);
    p_Grcg->mode = 0xc5;
    memset(p_Background->sources, 0, sizeof(p_Background->sources));
    p_Background->sources[9] = 72;
    memset(&p_Background->scroll, 0, sizeof(p_Background->scroll));
    p_Background->scroll.map_row = 91;
    p_Background->scroll.line = 7;
    memset(&p_Background->cache, 0xa5, sizeof(p_Background->cache));
    for (size_t page = 0; page < 2; page++) {
        for (size_t y = 0; y < 16; y++) {
            for (size_t x = 0; x < 16; x++) {
                g_Video.pages[page][(72 + y * 80) * 8 + x] =
                    1 + page * 4 + x / 8;
            }
        }
    }
    Background_SetTile(1, 0, 104, 35, 9);
    CHECK_EQ_INT(p_Background->cache.images[2][4], 9);
    CHECK_EQ_INT(p_Background->cache.images[2][5], 0xa5);
    CHECK_EQ_INT(p_Background->cache.dirty[4][2], 0xa5);
    CHECK_EQ_INT(p_Background->scroll.map_row, 91);
    CHECK_EQ_INT(p_Background->scroll.line, 7);
    CHECK_EQ_INT(p_Background->scroll.copy_top, 0);
    CHECK_EQ_INT(p_Background->scroll.copy_height, 16);
    for (size_t page = 0; page < 2; page++) {
        CHECK_EQ_INT(g_Video.pages[page][32 * 640 + 104], 2 + page * 4);
        CHECK_EQ_INT(g_Video.pages[page][32 * 640 + 112], 1 + page * 4);
    }
    CHECK_EQ_INT(g_Video.drawing_page, 0);
    CHECK_EQ_INT(p_Grcg->mode, 0);
    CHECK_EQ_INT(p_Grcg->tiles[0], 255);
    CHECK_EQ_INT(p_Grcg->tiles[1], 0);
    CHECK_EQ_INT(p_Grcg->tiles[3], 0);
    Arena_Free(&arena);
}

TEST(set_both_retains_ring_aliases_and_repeated_same_page_copies)
{
    ARENA arena;
    Arena_Init(&arena, "tile ring alias", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    Grcg_Init();
    memset(p_Background->sources, 0, sizeof(p_Background->sources));
    p_Background->sources[7] = 0;
    memset(&p_Background->scroll, 0, sizeof(p_Background->scroll));
    memset(&p_Background->cache, 0, sizeof(p_Background->cache));
    for (size_t y = 0; y < 16; y++) {
        memset(g_Video.pages[0] + y * 640, 3, 8);
        memset(g_Video.pages[0] + y * 640 + 8, 5, 8);
    }
    Background_SetTile(0, 0, 24, 0, 7);
    CHECK_EQ_INT(p_Background->cache.mode, 7);
    CHECK_EQ_INT(g_Video.pages[0][24], 5);
    CHECK_EQ_INT(g_Video.pages[0][32], 3);
    CHECK_EQ_INT(g_Video.drawing_page, 0);
    Arena_Free(&arena);
}

TEST(set_both_resolves_source_aliases_after_updating_copy_parameters_and_ring)
{
    ARENA arena;
    Arena_Init(&arena, "tile source aliases", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    Grcg_Init();
    memset(p_Background->sources, 0, sizeof(p_Background->sources));
    memset(&p_Background->scroll, 0, sizeof(p_Background->scroll));
    p_Background->scroll.copy_top = -1;
    p_Background->scroll.copy_height = -1;
    memset(&p_Background->cache, 0, sizeof(p_Background->cache));
    p_Background->cache.mode = TILE_IMAGES;
    for (size_t page = 0; page < 2; page++) {
        for (size_t row = 0; row < 16; row++) {
            memset(g_Video.pages[page] + (16 + row * 80) * 8, 7, 16);
            memset(g_Video.pages[page] + (0x7201 + row * 80) * 8, 9, 8);
            memset(g_Video.pages[page] + (0x7201 + row * 80) * 8 + 8, 11, 8);
        }
    }
    Background_SetTile(0, 1, 64, 32, 101);
    CHECK_EQ_INT(g_Video.pages[0][32 * 640 + 64], 7);
    CHECK_EQ_INT(g_Video.pages[1][32 * 640 + 64], 7);
    Background_SetTile(0, 1, 32, 0, 114);
    CHECK_EQ_INT(p_Background->cache.images[0][0], 114);
    CHECK_EQ_INT(g_Video.pages[0][32], 11);
    CHECK_EQ_INT(g_Video.pages[0][40], 9);
    CHECK_EQ_INT(g_Video.pages[1][32], 11);
    CHECK_EQ_INT(g_Video.pages[1][40], 9);
    Arena_Free(&arena);
}

TEST(sources_resolve_live_parameters_pending_row_and_cache)
{
    memset(p_Background->sources, 0, sizeof(p_Background->sources));
    p_Background->sources[99] = 0x4c00;
    memset(&p_Background->scroll, 0, sizeof(p_Background->scroll));
    p_Background->scroll.copy_top = -1;
    p_Background->scroll.copy_height = 16;
    TILE_SCROLL *const scroll = &p_Background->scroll;
    memset(&p_Background->cache, 0, sizeof(p_Background->cache));
    p_Background->cache.mode = TILE_IMAGES;
    TILE_CACHE *const cache = &p_Background->cache;
    CHECK_EQ_INT(Background_Source(99), 0x4c00);
    CHECK_EQ_INT(Background_Source(100), 0xffff);
    CHECK_EQ_INT(Background_Source(101), 16);
    scroll->copy_top = 4;
    scroll->row[0] = 0x34;
    scroll->row[1] = 0x12;
    cache->images[0][0] = 0x56;
    CHECK_EQ_INT(Background_Source(100), 4);
    CHECK_EQ_INT(Background_Source(102), 0x1234);
    CHECK_EQ_INT(Background_Source(114), 0x5601);
    cache->images[11][17] = 0x78;
    cache->images[11][18] = 0x12;
    CHECK_EQ_INT(Background_Source(255), 0x1278);
}

TEST(dirty_and_partial_copies_read_live_source_aliases)
{
    ARENA arena;
    Arena_Init(&arena, "live tile source reads", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    Grcg_Init();
    memset(p_Background->sources, 0, sizeof(p_Background->sources));
    memset(&p_Background->scroll, 0, sizeof(p_Background->scroll));
    p_Background->scroll.copy_height = 16;
    TILE_SCROLL *const scroll = &p_Background->scroll;
    memset(&p_Background->cache, 0, sizeof(p_Background->cache));
    p_Background->cache.mode = TILE_IMAGES;
    TILE_CACHE *const cache = &p_Background->cache;
    for (unsigned row = 0; row < 16; row++) {
        memset(g_Video.pages[0] + (16 + row * 80) * 8, 7, 16);
        memset(g_Video.pages[0] + (32 + row * 80) * 8, 9, 16);
    }
    cache->images[0][0] = 101;
    cache->columns[0] = cache->dirty[0][0] = 1;
    Background_RepaintDirty();
    CHECK_EQ_INT(g_Video.pages[0][32], 7);
    scroll->copy_height = 32;
    cache->columns[0] = cache->dirty[0][0] = 1;
    Background_RepaintDirty();
    CHECK_EQ_INT(g_Video.pages[0][32], 9);
    scroll->row[0] = 32;
    scroll->row[1] = 0;
    Render_CopyLines(80 * 100 + 4, 102, 1, 2, TILE_COPY_ALL);
    CHECK_EQ_INT(g_Video.pages[0][100 * VIDEO_WIDTH + 32], 9);
    CHECK_EQ_INT(g_Video.pages[0][101 * VIDEO_WIDTH + 32], 9);
    CHECK_EQ_INT(cache->images[0][0], 101);
    CHECK_EQ_INT(scroll->copy_height, 32);
    Arena_Free(&arena);
}
