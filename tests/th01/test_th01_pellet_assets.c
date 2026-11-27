#include "th01/pellet_assets.h"

#include "th01_sprites.h"
#include "test.h"

#include <string.h>

TEST(missing_sheets_leave_assets_empty)
{
    VFS *const vfs = Vfs_Create(&g_HeapAllocator);
    PELLET_ASSETS *assets = nullptr;
    CHECK(!Result_Ignore(PelletAssets_Load(vfs, nullptr, &assets)));
    CHECK(assets == nullptr);
    Vfs_Destroy(vfs);
}

TEST(pellets_load_pre_shifted_big_endian_rows)
{
    VFS *const vfs = Th01Test_Sprites();
    PELLET_ASSETS *assets = nullptr;
    CHECK(IS_OK(PelletAssets_Load(vfs, &g_HeapAllocator, &assets)));
    CHECK(
        memcmp(assets->pellets[0][0][0], (uint8_t[2]) { 0x3c, 0x00 }, 2) == 0);
    CHECK(
        memcmp(assets->pellets[0][7][2], (uint8_t[2]) { 0x01, 0xfe }, 2) == 0);
    CHECK(
        memcmp(assets->pellets[2][3][1], (uint8_t[2]) { 0x04, 0x80 }, 2) == 0);
    CHECK(memcmp(assets->pellets[2][0][0], (uint8_t[2]) {}, 2) == 0);
    CHECK(memcmp(assets->clouds[0][0], (uint8_t[2]) { 0x07, 0xe0 }, 2) == 0);
    CHECK(memcmp(assets->clouds[1][0], (uint8_t[2]) { 0x01, 0x80 }, 2) == 0);
    Mem_Free(assets);
    Vfs_Destroy(vfs);
}

TEST(shifted_masks_preserve_byte_order_and_row_overflow)
{
    Video_Init(&g_HeapAllocator);
    PELLET_ASSETS assets = {};
    assets.pellets[1][7][0][0] = 1;
    assets.pellets[1][7][0][1] = 0x80;
    PelletAssets_Put(&assets, 631, 100, 1);
    CHECK_EQ_INT(g_Video.pages[0][100 * 640 + 631], 7);
    CHECK_EQ_INT(g_Video.pages[0][100 * 640 + 632], 7);
    CHECK_EQ_INT(g_Video.pages[0][100 * 640 + 630], 0);
    assets.clouds[0][0][1] = 0x80;
    PelletAssets_Cloud(&assets, 639, 100, 0, false);
    CHECK_EQ_INT(g_Video.pages[0][101 * 640], 7);
    g_Video.pages[1][101 * 640] = 3;
    PelletAssets_Cloud(&assets, 639, 100, 0, true);
    CHECK_EQ_INT(g_Video.pages[0][101 * 640], 3);
    Mem_Free(g_Video.pages[0]);
}

TEST(clouds_wrap_planar_addresses_at_bottom)
{
    Video_Init(&g_HeapAllocator);
    PELLET_ASSETS assets = {};
    assets.clouds[1][10][0] = 0x80;
    PelletAssets_Cloud(&assets, 0, 400, 1, false);
    CHECK_EQ_INT(g_Video.pages[0][32 * 8], 7);
    g_Video.pages[1][32 * 8] = 15;
    PelletAssets_Cloud(&assets, 0, 400, 1, true);
    CHECK_EQ_INT(g_Video.pages[0][32 * 8], 6);
    Mem_Free(g_Video.pages[0]);
}

TEST(cloud_restore_clears_current_page_then_ors_and_keeps_empty_selector)
{
    Video_Init(&g_HeapAllocator);
    PELLET_ASSETS assets = {};
    Video_SelectDrawingPage(1);
    PelletAssets_Cloud(&assets, 100, 100, 0, true);
    CHECK_EQ_INT(g_Video.drawing_page, 1);
    assets.clouds[0][0][0] = 0x80;
    const int32_t pixel = 100 * 640 + 96;
    g_Video.pages[0][pixel] = 9;
    g_Video.pages[1][pixel] = 15;
    PelletAssets_Cloud(&assets, 100, 100, 0, true);
    CHECK_EQ_INT(g_Video.pages[1][pixel], 0);
    CHECK_EQ_INT(g_Video.pages[0][pixel], 9);
    CHECK_EQ_INT(g_Video.drawing_page, 0);
    g_Video.pages[0][pixel] = 8;
    PelletAssets_Cloud(&assets, 100, 100, 0, false);
    CHECK_EQ_INT(g_Video.pages[0][pixel], 7);
    Mem_Free(g_Video.pages[0]);
}
