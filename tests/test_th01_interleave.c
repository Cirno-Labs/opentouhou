#include "th01/interleave.h"

#include "core/base/arena.h"

#include "th01_video.h"
#include "test.h"

#include <string.h>

TEST(rejects_unknown_executable_before_allocating)
{
    READER reader;
    Reader_OpenBuffer(&reader, nullptr, 0, nullptr);
    INTERLEAVE_ASSETS *assets = nullptr;
    CHECK(!Result_Ignore(
        Interleave_LoadAssets(&reader, nullptr, 0, nullptr, &assets)));
    CHECK(assets == nullptr);
    ARENA arena;
    Arena_Init(&arena, "interleave", 1024 * 1024);
    uint8_t *const file =
        Allocator_AllocZ(Arena_Allocator(&arena), REIIDEN_IMAGE_SIZE);
    uint8_t *const scratch =
        Allocator_Alloc(Arena_Allocator(&arena), REIIDEN_IMAGE_SIZE);
    Reader_OpenBuffer(&reader, file, REIIDEN_IMAGE_SIZE, nullptr);
    CHECK(!Result_Ignore(
        Interleave_LoadAssets(&reader, nullptr, 0, nullptr, &assets)));
    CHECK_EQ_INT(Reader_Tell(&reader), 0);
    CHECK(!Result_Ignore(Interleave_LoadAssets(
        &reader, scratch, REIIDEN_IMAGE_SIZE, nullptr, &assets)));
    CHECK(assets == nullptr);
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
