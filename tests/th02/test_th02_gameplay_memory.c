#include "tests/test.h"
#include "th02/gameplay/memory.h"
#include "th02/video.h"

#include "core/base/arena.h"
#include "core/base/memory.h"

#include <string.h>

static void M_Begin(const int32_t height)
{
    GAMEPLAY_ASSETS *const assets = Mem_AllocZ(sizeof(*assets));
    BOMB_ASSETS bomb = {};
    bomb.illustration.width = 640;
    bomb.illustration.height = height;
    GameplayMemory_Begin(assets, &bomb, false);
    Mem_Free(assets);
}

TEST(page_copy_failure_preserves_pixels_and_page_selection)
{
    ARENA arena;
    Arena_Init(&arena, "TH02 scratch test", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    memset(g_Video.pages[0], 7, VIDEO_PAGE_SIZE);
    memset(g_Video.pages[1], 3, VIDEO_PAGE_SIZE);
    Video_AccessPage(1);
    Video_ShowPage(1);
    M_Begin(780);
    CHECK(!Result_Ignore(GameplayMemory_CopyPage(0)));
    CHECK_EQ_INT(g_Video.drawing_page, 1);
    CHECK_EQ_INT(g_Video.visible_page, 1);
    CHECK_EQ_INT(g_Video.pages[0][0], 7);
    CHECK_EQ_INT(g_Video.pages[1][0], 3);
    M_Begin(720);
    CHECK(Result_Ignore(GameplayMemory_CopyPage(0)));
    CHECK_EQ_INT(g_Video.drawing_page, 0);
    CHECK_EQ_INT(g_Video.visible_page, 1);
    CHECK_EQ_INT(g_Video.pages[0][0], 3);
    CHECK_EQ_INT(g_Video.pages[0][VIDEO_VISIBLE_SIZE], 7);
    GameplayMemory_Shutdown();
    Arena_Free(&arena);
}

TEST(freed_stage_payload_does_not_count_as_contiguous_scratch)
{
    ARENA arena;
    Arena_Init(&arena, "TH02 fragmented scratch test", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    M_Begin(560);
    CHECK(Result_Ignore(GameplayMemory_CopyPage(0)));
    STAGE_ASSETS stage = {};
    PATTERN_BANK patterns = {};
    stage.pattern_first = 128;
    stage.data.payload_size = 65535;
    stage.data.declared_line_count = 40;
    GameplayMemory_LoadStage(&stage, &patterns);
    CHECK(!Result_Ignore(GameplayMemory_CopyPage(0)));
    stage.data.payload_size = 0;
    stage.data.declared_line_count = 0;
    GameplayMemory_LoadStage(&stage, &patterns);
    CHECK(Result_Ignore(GameplayMemory_CopyPage(0)));
    GameplayMemory_Shutdown();
    Arena_Free(&arena);
}

TEST(leaving_gameplay_releases_copy_capacity_restrictions)
{
    ARENA arena;
    Arena_Init(&arena, "TH02 released scratch test", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    M_Begin(780);
    CHECK(!Result_Ignore(GameplayMemory_CopyPage(0)));
    GameplayMemory_Shutdown();
    CHECK(Result_Ignore(GameplayMemory_CopyPage(0)));
    Arena_Free(&arena);
}
