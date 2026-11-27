#include "tests/test.h"
#include "tests/th02/th02_clip_fixture.h"
#include "tests/th02/th02_player_fixture.h"
#include "tests/th02/th02_sound.h"
#include "th02/background.h"
#include "th02/background/private.h"
#include "th02/player.h"

#include "core/base/arena.h"
#include "core/base/memory.h"

#include <string.h>

TEST(player_draws_and_cleans_rolling_body_and_options_on_the_selected_page)
{
    ARENA arena;
    Arena_Init(&arena, "TH02 player render test", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    memset(g_Video.pages[0], 6, VIDEO_PAGE_SIZE);
    memset(g_Video.pages[1], 12, VIDEO_PAGE_SIZE);
    Video_AccessPage(1);
    Video_SetSplit(123, 456, 789);
    TestClip_Set(0, 0, 639, 399);
    Grcg_Init();

    PATTERN_BANK patterns;
    PatternBank_Init(&patterns, &g_HeapAllocator);
    uint8_t body[32 * 48];
    uint8_t option[16 * 16];
    memset(body, 5, sizeof(body));
    body[0] = 0;
    memset(option, 9, sizeof(option));
    CHECK(Result_Ignore(PatternBank_Put(&patterns, 0, 32, 48, body, 0)));
    CHECK(Result_Ignore(PatternBank_Put(&patterns, 72, 16, 16, option, 0)));
    CHECK(Result_Ignore(PatternBank_ConvertTiny(&patterns, 72)));
    for (uint16_t sprite = 52; sprite <= 53; sprite++) {
        CHECK(Result_Ignore(
            PatternBank_Put(&patterns, sprite, 16, 16, option, 0)));
        CHECK(Result_Ignore(PatternBank_ConvertTiny(&patterns, sprite)));
    }
    memset(p_Background, 0, sizeof(*p_Background));
    p_Background->cache.mode = TILE_BLACK;
    int16_t scroll = 63;
    TEST_SOUND sound;
    TestSound_Init(&sound);
    Player_Init();
    Player_ResetPosition();
    m_Player.power = 8;
    const PLAYFIELD_PAGES pages = { 1, 0, scroll };
    Player_Controls(pages, GAME_KEY_SHOT);
    CHECK_EQ_INT(m_Shots.slots[0].state, ENTITY_ACTIVE);
    CHECK_EQ_INT(m_Shots.slots[0].position[1].x, 216 * 16);
    CHECK_EQ_INT(m_Shots.slots[0].position[0].x, 0);
    TestSound_Check(&sound, -1);
    SoundEffects_Update();
    TestSound_Check(&sound, 1);
    CHECK(!Player_Update(pages, 0, false, &patterns, 0));
    CHECK_EQ_INT(g_Video.pages[1][399 * 640 + 208], 12);
    CHECK_EQ_INT(g_Video.pages[1][399 * 640 + 209], 5);
    CHECK_EQ_INT(g_Video.pages[1][46 * 640 + 239], 5);
    CHECK_EQ_INT(g_Video.pages[1][15 * 640 + 192], 9);
    CHECK_EQ_INT(g_Video.pages[1][30 * 640 + 255], 9);
    CHECK_EQ_INT(g_Grcg->mode, 0);
    CHECK_EQ_INT(g_Grcg->tiles[0], 255);
    CHECK_EQ_INT(g_Grcg->tiles[1], 0);
    CHECK_EQ_INT(g_Grcg->tiles[2], 0);
    CHECK_EQ_INT(g_Grcg->tiles[3], 255);
    Player_Invalidate(pages, false);
    Background_RepaintDirty();
    CHECK_EQ_INT(g_Video.pages[1][399 * 640 + 209], 0);
    CHECK_EQ_INT(g_Video.pages[1][46 * 640 + 239], 0);
    CHECK_EQ_INT(g_Video.pages[1][15 * 640 + 192], 0);
    CHECK_EQ_INT(g_Video.pages[1][30 * 640 + 255], 0);
    CHECK_EQ_INT(g_Video.pages[1][100 * 640 + 208], 12);
    for (size_t pixel = 0; pixel < VIDEO_PAGE_SIZE; pixel++) {
        CHECK_EQ_INT(g_Video.pages[0][pixel], 6);
    }
    CHECK_EQ_INT(g_Video.drawing_page, 1);
    CHECK_EQ_INT(g_Video.visible_page, 0);
    CHECK_EQ_INT(g_Video.first_lines, 123);
    CHECK_EQ_INT(g_Video.first_address, 456);
    CHECK_EQ_INT(g_Video.second_address, 789);
    PatternBank_Reset(&patterns);
    TestSound_Free(&sound);
    Arena_Free(&arena);
}
