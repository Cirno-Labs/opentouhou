#include "th01/pellet_assets.h"

#include "test.h"

TEST(rejects_unknown_executable_without_allocation)
{
    READER reader;
    Reader_OpenBuffer(&reader, nullptr, 0, nullptr);
    PELLET_ASSETS *assets = nullptr;
    CHECK(!Result_Ignore(
        PelletAssets_Load(&reader, nullptr, 0, nullptr, &assets)));
    CHECK(assets == nullptr);
}

TEST(shifted_masks_preserve_byte_order_and_row_overflow)
{
    VIDEO video;
    Video_Init(&video, &g_HeapAllocator);
    PELLET_ASSETS assets = {};
    assets.pellets[1][7][0][0] = 1;
    assets.pellets[1][7][0][1] = 0x80;
    PelletAssets_Put(&video, &assets, 631, 100, 1);
    CHECK_EQ_INT(video.pages[0][100 * 640 + 631], 15);
    CHECK_EQ_INT(video.pages[0][100 * 640 + 632], 15);
    CHECK_EQ_INT(video.pages[0][100 * 640 + 630], 0);
    assets.clouds[0][0][1] = 0x80;
    PelletAssets_Cloud(&video, &assets, 639, 100, 0, false);
    CHECK_EQ_INT(video.pages[0][101 * 640], 15);
    video.pages[1][101 * 640] = 3;
    PelletAssets_Cloud(&video, &assets, 639, 100, 0, true);
    CHECK_EQ_INT(video.pages[0][101 * 640], 3);
    Mem_Free(video.pages[0]);
}

TEST(clouds_wrap_planar_addresses_at_bottom)
{
    VIDEO video;
    Video_Init(&video, &g_HeapAllocator);
    PELLET_ASSETS assets = {};
    assets.clouds[1][10][0] = 0x80;
    PelletAssets_Cloud(&video, &assets, 0, 400, 1, false);
    CHECK_EQ_INT(video.pages[0][32 * 8], 15);
    video.pages[1][32 * 8] = 15;
    PelletAssets_Cloud(&video, &assets, 0, 400, 1, true);
    CHECK_EQ_INT(video.pages[0][32 * 8], 6);
    Mem_Free(video.pages[0]);
}
