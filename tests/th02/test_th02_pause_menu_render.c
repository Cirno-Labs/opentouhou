#include "tests/test.h"
#include "tests/test_allocator.h"
#include "tests/th02/th02_palette_fixture.h"
#include "tests/th02/th02_pause_menu.h"
#include "tests/th02/th02_tram_fixture.h"
#include "th02/pause_menu.h"
#include "th02/pause_menu/private.h"

#include "core/base/arena.h"

#include <string.h>

TEST(pause_draws_cached_labels_and_distinct_heading_and_blank_widths)
{
    static const TRAM_CHARACTER characters[] = {
        { .code = 'R', .source_end = 1 }, { .code = 'Q', .source_end = 1 },
        { .code = '?', .source_end = 1 }, { .code = 'N', .source_end = 1 },
        { .code = 'Y', .source_end = 1 },
    };
    PAUSE_MENU_ASSETS assets = {};
    for (size_t i = 0; i < 5; i++) {
        assets.labels[i] = (TRAM_TEXT) {
            .text = "",
            .characters = &characters[i],
            .count = 1,
            .source_size = 1,
        };
    }
    ARENA arena;
    Arena_Init(&arena, "TH02 pause rendering test", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    memset(g_Video.pages[0], 3, VIDEO_PAGE_SIZE);
    memset(g_Video.pages[1], 12, VIDEO_PAGE_SIZE);
    Video_AccessPage(1);
    Video_ShowPage(1);
    Video_SetSplit(123, 456, 789);
    memset(p_Palette, 0, sizeof((*p_Palette)));
    memset(p_Palette->colors, 255, sizeof(p_Palette->colors));

    memset(p_Tram, 0xa5, sizeof(*p_Tram));
    TestPause_Init();
    PauseMenu_Begin(&assets, 0);
    CHECK_EQ_INT(p_Tram->codes[15 * 80 + 23], 'R');
    CHECK_EQ_INT(p_Tram->attributes[15 * 80 + 23], 0xe9);
    CHECK_EQ_INT(p_Tram->codes[16 * 80 + 23], 'Q');
    CHECK_EQ_INT(p_Tram->attributes[16 * 80 + 23], 0x61);
    CHECK_EQ_INT(p_Palette->tone, 70);
    CHECK_EQ_INT(g_Video.palette[15].red, 170);
    CHECK_EQ_INT(PauseMenu_Update(0).result, PAUSE_MENU_INPUT);
    CHECK_EQ_INT(p_Tram->codes[12 * 80 + 18], 0x3957);
    CHECK_EQ_INT(p_Tram->codes[12 * 80 + 30], 0x3757);
    CHECK_EQ_INT(p_Tram->codes[12 * 80 + 34], 0x3657);
    CHECK_EQ_INT(p_Tram->codes[12 * 80 + 38], 0xa5a5);
    CHECK_EQ_INT(PauseMenu_Update(0).result, PAUSE_MENU_FRAME);
    p_Pause->frame = 50;
    CHECK_EQ_INT(PauseMenu_Update(0).result, PAUSE_MENU_INPUT);
    CHECK_EQ_INT(p_Tram->codes[12 * 80 + 38], 0x4f57);
    CHECK_EQ_INT(p_Tram->codes[12 * 80 + 39], 0xcf57);
    CHECK_EQ_INT(p_Tram->codes[12 * 80 + 40], 0xa5a5);
    p_Pause->state = 2;
    CHECK_EQ_INT(PauseMenu_Update(0).result, PAUSE_MENU_RESUME);
    CHECK_EQ_INT(p_Palette->tone, 100);
    CHECK_EQ_INT(g_Video.palette[15].red, 255);
    CHECK_EQ_INT(p_Tram->codes[15 * 80 + 23], 0x4f57);
    CHECK_EQ_INT(p_Tram->codes[16 * 80 + 23], 0x4f57);
    CHECK_EQ_INT(p_Tram->codes[2000], 0xa5a5);
    CHECK_EQ_INT(p_Tram->attributes[4095], 0xa5a5);
    CHECK_EQ_INT(g_Video.drawing_page, 1);
    CHECK_EQ_INT(g_Video.visible_page, 1);
    CHECK_EQ_INT(g_Video.first_lines, 123);
    CHECK_EQ_INT(g_Video.first_address, 456);
    CHECK_EQ_INT(g_Video.second_address, 789);
    for (size_t pixel = 0; pixel < VIDEO_PAGE_SIZE; pixel++) {
        CHECK_EQ_INT(g_Video.pages[0][pixel], 3);
        CHECK_EQ_INT(g_Video.pages[1][pixel], 12);
    }
    for (size_t color = 0; color < 16; color++) {
        for (size_t channel = 0; channel < 3; channel++) {
            CHECK_EQ_INT(p_Palette->colors[color][channel], 255);
        }
    }
    Arena_Free(&arena);
}

TEST(pause_labels_decode_the_built_in_text_and_free_cleanly)
{
    TEST_ALLOCATOR allocator;
    TestAllocator_Init(&allocator);
    PAUSE_MENU_ASSETS assets;
    CHECK(Result_Ignore(PauseMenuAssets_Load(&allocator.allocator, &assets)));
    CHECK_EQ_STR(assets.labels[4].text, "　　はい、やめます。　");
    for (size_t i = 0; i < ARRAY_SIZE(assets.labels); i++) {
        CHECK(assets.labels[i].count > 0);
    }
    PauseMenuAssets_Free(&assets);
    CHECK_EQ_INT(allocator.live_blocks, 0);
    CHECK_EQ_INT(allocator.size_errors, 0);
    PauseMenuAssets_Free(&assets);
}
