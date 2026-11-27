#include "th02/background.h"
#include "th02/background/private.h"
#include "th02/grcg.h"
#include "th02/video.h"

#include "core/base/arena.h"

#include "test.h"

TEST(cache_reset_keeps_ring_and_dirty_bytes)
{
    memset(&p_Background->cache, 0xa5, sizeof(p_Background->cache));
    Background_Reset();
    CHECK_EQ_INT(p_Background->cache.mode, TILE_IMAGES);
    CHECK_EQ_INT(p_Background->cache.redraw_all, 0);
    CHECK_EQ_INT(p_Background->cache.images[24][23], 0xa5);
    CHECK_EQ_INT(p_Background->cache.reserved, 0xa5);
    CHECK_EQ_INT(p_Background->cache.dirty[0][0], 0xa5);
    CHECK_EQ_INT(p_Background->cache.columns[23], 0xa5);
}

TEST(aligned_rectangles_and_vertical_wrap_mark_exact_rows)
{
    memset(&p_Background->cache, 0, sizeof(p_Background->cache));
    p_Background->cache.mode = TILE_IMAGES;
    Background_Mark(0, 32, 16, 16, 16);
    CHECK_EQ_INT(p_Background->cache.columns[0], 1);
    CHECK_EQ_INT(p_Background->cache.columns[1], 0);
    CHECK_EQ_INT(p_Background->cache.dirty[0][0], 0);
    CHECK_EQ_INT(p_Background->cache.dirty[0][1], 1);
    CHECK_EQ_INT(p_Background->cache.dirty[0][2], 0);
    Background_Mark(399, 32, 0, 16, 16);
    CHECK_EQ_INT(p_Background->cache.dirty[0][24], 1);
    CHECK_EQ_INT(p_Background->cache.dirty[0][0], 1);
    CHECK_EQ_INT(p_Background->cache.reserved, 0);
}

TEST(cache_aliases_keep_mode_ring_and_redraw_effects)
{
    memset(&p_Background->cache, 0, sizeof(p_Background->cache));
    p_Background->cache.mode = TILE_BLACK;
    Background_Mark(0, 32, -9632, 16, 1);
    CHECK_EQ_INT(p_Background->cache.mode, TILE_IMAGES);
    Background_Mark(0, 32, -9616, 16, 1);
    CHECK_EQ_INT(p_Background->cache.images[0][0], 1);
    Background_Mark(0, 400, 1584, 16, 1);
    CHECK_EQ_INT(p_Background->cache.redraw_all, 1);
    ARENA arena;
    Arena_Init(&arena, "cache alias drawing", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    Grcg_Init();
    memset(p_Background->sources, 0, sizeof(p_Background->sources));
    p_Background->sources[1] = 72;
    for (size_t y = 0; y < 400; y++) {
        memset(g_Video.pages[0] + y * VIDEO_WIDTH + 32, 7, 384);
    }
    memset(g_Video.pages[0] + 576, 9, 16);
    Background_RepaintDirty();
    for (size_t y = 0; y < 400; y++) {
        for (size_t x = 32; x < 416; x++) {
            CHECK_EQ_INT(
                g_Video.pages[0][y * VIDEO_WIDTH + x],
                y == 0 && x < 48 ? 9 : 0);
        }
    }
    CHECK_EQ_INT(g_Grcg->mode, 0);
    CHECK_EQ_INT(p_Background->cache.redraw_all, 0);
    CHECK_EQ_INT(p_Background->cache.images[0][0], 1);
    memset(&p_Background->cache, 0, sizeof(p_Background->cache));
    p_Background->cache.mode = TILE_IMAGES;
    Background_Mark(0, 48, -16, 16, 1);
    CHECK_EQ_INT(p_Background->cache.dirty[0][24], 1);
    CHECK_EQ_INT(p_Background->cache.columns[0], 0);
    CHECK_EQ_INT(p_Background->cache.columns[1], 1);
    memset(g_Video.pages[0], 5, VIDEO_PAGE_SIZE);
    Background_RepaintDirty();
    for (size_t pixel = 0; pixel < VIDEO_PAGE_SIZE; pixel++) {
        CHECK_EQ_INT(g_Video.pages[0][pixel], 5);
    }
    Arena_Free(&arena);
}

TEST(flush_uses_column_order_exact_all_flag_and_black_charger_sequence)
{
    memset(&p_Background->cache, 0, sizeof(p_Background->cache));
    p_Background->cache.mode = TILE_IMAGES;
    p_Background->cache.columns[0] = p_Background->cache.columns[1] = 1;
    p_Background->cache.dirty[0][1] = p_Background->cache.dirty[1][0] = 1;
    p_Background->cache.images[1][0] = 7;
    p_Background->cache.images[0][1] = 9;
    ARENA arena;
    Arena_Init(&arena, "cache copy order", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    Grcg_Init();
    memset(p_Background->sources, 0, sizeof(p_Background->sources));
    p_Background->sources[7] = 6;
    p_Background->sources[9] = 72;
    memset(g_Video.pages[0] + 48, 3, 16);
    memset(g_Video.pages[0] + 576, 9, 16);
    Background_RepaintDirty();
    CHECK_EQ_INT(g_Video.pages[0][16 * 640 + 32], 3);
    CHECK_EQ_INT(g_Video.pages[0][48], 9);
    p_Background->cache.redraw_all = 2;
    memset(g_Video.pages[0], 5, VIDEO_PAGE_SIZE);
    Background_RepaintDirty();
    for (size_t pixel = 0; pixel < VIDEO_PAGE_SIZE; pixel++) {
        CHECK_EQ_INT(g_Video.pages[0][pixel], 5);
    }
    CHECK_EQ_INT(p_Background->cache.redraw_all, 0);
    p_Background->cache.mode = TILE_BLACK;
    p_Background->cache.columns[0] = p_Background->cache.dirty[0][1] = 1;
    Background_RepaintDirty();
    CHECK_EQ_INT(g_Video.pages[0][16 * 640 + 32], 0);
    CHECK_EQ_INT(g_Video.pages[0][16 * 640 + 31], 5);
    CHECK_EQ_INT(g_Video.pages[0][16 * 640 + 48], 5);
    CHECK_EQ_INT(g_Grcg->mode, 0);
    for (size_t plane = 0; plane < 4; plane++) {
        CHECK_EQ_INT(g_Grcg->tiles[plane], 0);
    }
    CHECK_EQ_INT(p_Background->cache.dirty[0][1], 0);
    CHECK_EQ_INT(p_Background->cache.columns[0], 0);
    p_Background->cache.mode = TILE_DISABLED;
    p_Background->cache.redraw_all = p_Background->cache.columns[0] =
        p_Background->cache.dirty[0][0] = 1;
    const TILE_CACHE before = p_Background->cache;
    Background_Mark(0, 32, -16, 16, 1);
    CHECK(
        memcmp(&p_Background->cache, &before, sizeof(p_Background->cache))
        == 0);
    Grcg_SetColor(5);
    const GRCG charger = *g_Grcg;
    Background_RepaintDirty();
    CHECK(memcmp(&charger, g_Grcg, sizeof(charger)) == 0);
    CHECK_EQ_INT(g_Video.pages[0][32], 5);
    CHECK_EQ_INT(p_Background->cache.redraw_all, 0);
    CHECK_EQ_INT(p_Background->cache.dirty[0][0], 0);
    Arena_Free(&arena);
}
