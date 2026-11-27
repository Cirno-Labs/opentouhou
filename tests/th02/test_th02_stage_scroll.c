#include "th02/background/private.h"
#include "th02/grcg.h"
#include "th02/stage/private.h"
#include "th02/stage/scroll.h"
#include "th02/video.h"

#include "core/base/arena.h"

#include "test.h"

TEST(cleanup_uses_the_page_position_and_movement_uses_the_saved_position)
{
    memset(&p_Stage->scroll, 0, sizeof(p_Stage->scroll));
    p_Stage->scroll.line = 100;
    p_Stage->scroll.page_lines[0] = 200;
    p_Stage->scroll.page_lines[1] = 300;
    p_Stage->scroll.speed = 1;
    p_Stage->scroll.interval = 4;
    StageScroll_Begin();
    CHECK_EQ_INT(p_Stage->scroll.previous, 100);
    CHECK_EQ_INT(p_Stage->scroll.line, 200);
    StageScroll_Advance();
    CHECK_EQ_INT(p_Stage->scroll.line, 99);
    CHECK_EQ_INT(p_Stage->scroll.page_lines[0], 99);
    CHECK_EQ_INT(p_Stage->scroll.page_lines[1], 300);
    CHECK_EQ_INT(p_Stage->scroll.moved, 1);
    p_Stage->scroll.back = 1;
    p_Stage->scroll.cycle = 1;
    StageScroll_Begin();
    CHECK_EQ_INT(p_Stage->scroll.line, 300);
    StageScroll_Advance();
    CHECK_EQ_INT(p_Stage->scroll.line, 99);
    CHECK_EQ_INT(p_Stage->scroll.page_lines[1], 99);
    CHECK_EQ_INT(p_Stage->scroll.moved, 0);
    StageScroll_InitPages();
    CHECK_EQ_INT(p_Stage->scroll.page_lines[0], 0);
    CHECK_EQ_INT(p_Stage->scroll.page_lines[1], 0);
    CHECK_EQ_INT(p_Stage->scroll.line, 99);
    CHECK_EQ_INT(p_Stage->scroll.cycle, 1);
}

TEST(map_end_keeps_the_display_split_and_skips_one_cycle_increment)
{
    ARENA arena;
    Arena_Init(&arena, "TH02 stage scroll end test", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    Video_SetSplit(123, 456, 789);

    Grcg_SetColor(5);
    memset(&p_Background->scroll, 0, sizeof(p_Background->scroll));
    p_Background->scroll.map_row = 23;
    p_Background->scroll.line = 0;
    memset(&p_Background->cache, 0, sizeof(p_Background->cache));
    memset(&p_Background->map, 0, sizeof(p_Background->map));
    p_Background->map.count = 3;
    memset(&p_Stage->scroll, 0, sizeof(p_Stage->scroll));
    p_Stage->scroll.line = 399;
    p_Stage->scroll.page_lines[0] = 399;
    p_Stage->scroll.page_lines[1] = 0;
    p_Stage->scroll.speed = 1;
    p_Stage->scroll.interval = 4;
    p_Stage->scroll.cycle = 255;
    p_Stage->scroll.moved = 1;
    p_Stage->scroll.front = 1;
    StageScroll_Finish();
    CHECK_EQ_INT(p_Stage->scroll.line, 400);
    CHECK_EQ_INT(p_Stage->scroll.page_lines[0], 399);
    CHECK_EQ_INT(p_Stage->scroll.address, 15960);
    CHECK_EQ_INT(p_Stage->scroll.done, 1);
    CHECK_EQ_INT(p_Stage->scroll.cycle, 255);
    CHECK_EQ_INT(g_Video.first_lines, 123);
    CHECK_EQ_INT(g_Video.first_address, 456);
    CHECK_EQ_INT(g_Video.second_address, 789);
    CHECK_EQ_INT(g_Grcg->mode, 0);
    CHECK_EQ_INT(g_Grcg->tiles[0], 255);
    StageScroll_Swap();
    CHECK_EQ_INT(p_Stage->scroll.front, 0);
    CHECK_EQ_INT(p_Stage->scroll.back, 1);
    CHECK_EQ_INT(g_Video.visible_page, 0);
    CHECK_EQ_INT(g_Video.drawing_page, 1);
    StageScroll_Begin();
    CHECK_EQ_INT(p_Stage->scroll.line, 0);
    StageScroll_Advance();
    CHECK_EQ_INT(p_Stage->scroll.line, 400);
    CHECK_EQ_INT(p_Stage->scroll.page_lines[1], 400);
    CHECK_EQ_INT(p_Stage->scroll.moved, 0);
    StageScroll_Finish();
    CHECK_EQ_INT(p_Stage->scroll.cycle, 0);
    Arena_Free(&arena);
}

TEST(scrolling_updates_both_pages_boundary_flags_and_the_display_split)
{
    ARENA arena;
    Arena_Init(&arena, "TH02 stage scroll draw test", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    for (size_t page = 0; page < 2; page++) {
        memset(g_Video.pages[page] + 576, 10 + page, 16);
    }
    Grcg_Init();
    memset(p_Background->sources, 0, sizeof(p_Background->sources));
    p_Background->sources[0] = 72;
    memset(&p_Background->scroll, 0, sizeof(p_Background->scroll));
    p_Background->scroll.line = 1;
    memset(&p_Background->cache, 0, sizeof(p_Background->cache));
    memset(&p_Stage->scroll, 0, sizeof(p_Stage->scroll));
    p_Stage->scroll.line = 0;
    p_Stage->scroll.rows = UINT16_MAX;
    p_Stage->scroll.speed = 1;
    p_Stage->scroll.cycle = 255;
    p_Stage->scroll.moved = 1;
    p_Stage->scroll.front = 1;
    StageScroll_Finish();
    CHECK_EQ_INT(p_Stage->scroll.rows, 0);
    CHECK_EQ_INT(p_Stage->scroll.row_changed, 1);
    CHECK_EQ_INT(p_Stage->scroll.address, 0);
    CHECK_EQ_INT(p_Stage->scroll.cycle, 0);
    CHECK_EQ_INT(p_Stage->scroll.done, 0);
    CHECK_EQ_INT(g_Video.first_lines, 400);
    CHECK_EQ_INT(g_Video.first_address, 0);
    CHECK_EQ_INT(g_Video.second_address, 0);
    CHECK_EQ_INT(g_Video.pages[0][16], 10);
    CHECK_EQ_INT(g_Video.pages[0][415], 10);
    CHECK_EQ_INT(g_Video.pages[1][16], 0);
    CHECK_EQ_INT(g_Video.pages[1][32], 11);
    CHECK_EQ_INT(g_Video.pages[1][415], 11);
    Arena_Free(&arena);
}

TEST(finish_uses_live_speed_after_movement_without_changing_the_saved_move)
{
    ARENA arena;
    Arena_Init(&arena, "live scroll speed", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    Stage_Init();
    Grcg_Init();
    for (size_t page = 0; page < 2; page++) {
        memset(g_Video.pages[page], 5, VIDEO_PAGE_SIZE);
        for (size_t row = 0; row < 16; row++) {
            memset(g_Video.pages[page] + (72 + row * 80) * 8, row + 1, 16);
        }
    }
    p_Background->sources[0] = 72;
    p_Stage->scroll.line = 100;
    p_Stage->scroll.page_lines[0] = 42;
    p_Stage->scroll.speed = 2;
    p_Stage->scroll.interval = 1;
    p_Stage->scroll.front = 1;
    p_Background->scroll.line = 2;
    StageScroll_Begin();
    CHECK_EQ_INT(p_Stage->scroll.line, 42);
    StageScroll_Advance();
    CHECK_EQ_INT(p_Stage->scroll.line, 98);
    CHECK_EQ_INT(p_Stage->scroll.moved, 2);
    CHECK_EQ_INT(p_Stage->scroll.page_lines[0], 98);
    p_Stage->scroll.speed = 1;
    StageScroll_Finish();
    CHECK_EQ_INT(p_Background->scroll.copy_top, 1);
    CHECK_EQ_INT(p_Background->scroll.copy_height, 1);
    CHECK_EQ_INT(p_Stage->scroll.moved, 2);
    CHECK_EQ_INT(p_Stage->scroll.line, 98);
    CHECK_EQ_INT(p_Stage->scroll.cycle, 1);
    CHECK_EQ_INT(g_Video.first_lines, 302);
    CHECK_EQ_INT(g_Video.first_address, 3920);
    CHECK_EQ_INT(g_Video.drawing_page, 0);
    for (size_t page = 0; page < 2; page++) {
        CHECK_EQ_INT(g_Video.pages[page][98 * VIDEO_WIDTH + 32], 2);
        CHECK_EQ_INT(g_Video.pages[page][99 * VIDEO_WIDTH + 32], 5);
    }
    CHECK_EQ_INT(g_Video.pages[0][98 * VIDEO_WIDTH + 16], 2);
    CHECK_EQ_INT(g_Video.pages[1][98 * VIDEO_WIDTH + 16], 5);
    CHECK_EQ_INT(g_Grcg->mode, 0);
    Arena_Free(&arena);
}
