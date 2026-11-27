#include "tests/test.h"
#include "tests/th01/th01_sprites.h"
#include "tests/th01/th01_video.h"
#include "th01/interleave.h"

#include "core/base/arena.h"
#include "core/base/memory.h"

#include <string.h>

TEST(missing_sheet_leaves_assets_empty)
{
    VFS *const vfs = Vfs_Create(&g_HeapAllocator);
    INTERLEAVE_ASSETS *assets = nullptr;
    CHECK(!Result_Ignore(Interleave_LoadAssets(vfs, nullptr, &assets)));
    CHECK(assets == nullptr);
    Vfs_Destroy(vfs);
}

TEST(loads_the_masks_from_the_built_in_sheet)
{
    ARENA arena;
    Arena_Init(&arena, "interleave", 1024 * 1024);
    VFS *const vfs = Th01Test_Sprites();
    INTERLEAVE_ASSETS *assets = nullptr;
    CHECK(IS_OK(Interleave_LoadAssets(vfs, Arena_Allocator(&arena), &assets)));
    const uint8_t second[8] = {
        0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa
    };
    const uint8_t fourth[8] = {
        0x00, 0xdd, 0x00, 0x77, 0x00, 0xdd, 0x00, 0x77
    };
    CHECK(memcmp(assets->masks[0], (uint8_t[8]) {}, 8) == 0);
    CHECK(memcmp(assets->masks[1], second, 8) == 0);
    CHECK(memcmp(assets->masks[3], fourth, 8) == 0);
    CHECK(memcmp(assets->masks[9], (uint8_t[8]) {}, 8) == 0);
    Vfs_Destroy(vfs);
    Arena_Free(&arena);
}

TEST(mixed_rows_choose_whole_indices_and_preserve_other_state)
{
    ARENA arena;
    Arena_Init(&arena, "interleave", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    memset(g_Video.pages[0], 9, VIDEO_PAGE_SIZE);
    memset(g_Video.pages[1], 6, VIDEO_PAGE_SIZE);
    INTERLEAVE_ASSETS assets = {};
    assets.masks[3][0] = 0xaa;
    assets.masks[3][1] = 0x55;
    assets.masks[3][2] = 0xff;
    Video_SelectDrawingPage(1);
    Video_SelectVisiblePage(1);
    Video_SetScroll(37);
    Video_SetDac(2, 3, 4, 5);
    const GFX_COLOR color = g_Video.palette[2];
    Interleave_Put8(&assets, 15, 100, 3);
    for (int32_t bit = 0; bit < 8; bit++) {
        CHECK_EQ_INT(Th01Test_Pixel(0, 8 + bit, 100), bit % 2 == 0 ? 9 : 6);
        CHECK_EQ_INT(Th01Test_Pixel(0, 8 + bit, 101), bit % 2 == 0 ? 6 : 9);
        CHECK_EQ_INT(Th01Test_Pixel(0, 8 + bit, 102), 9);
        CHECK_EQ_INT(Th01Test_Pixel(0, 8 + bit, 103), 6);
        CHECK_EQ_INT(Th01Test_Pixel(1, 8 + bit, 100), 6);
    }
    CHECK_EQ_INT(Th01Test_Pixel(0, 7, 100), 9);
    CHECK_EQ_INT(Th01Test_Pixel(0, 16, 100), 9);
    CHECK_EQ_INT(Th01Test_Pixel(0, 8, 108), 9);
    CHECK_EQ_INT(g_Video.drawing_page, 0);
    CHECK_EQ_INT(g_Video.visible_page, 1);
    CHECK_EQ_INT(g_Video.scroll_top, 37);
    CHECK(memcmp(&g_Video.palette[2], &color, sizeof(color)) == 0);
    Interleave_Put8(&assets, 639, 392, 0);
    CHECK_EQ_INT(Th01Test_Pixel(0, 639, 399), 6);
    CHECK_EQ_INT(Th01Test_Pixel(0, 631, 399), 9);
    Arena_Free(&arena);
}
