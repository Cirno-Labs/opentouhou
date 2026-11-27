#include "th02_player_fixture.h"
#include "th02_clip_fixture.h"
#include "th02/background.h"
#include "th02/background/private.h"
#include "th02/player.h"
#include "th02/shots.h"

#include "core/base/arena.h"

#include "test.h"

TEST(shot_body_option_and_big_hit_wrap_and_clear_on_the_drawing_page)
{
    ARENA arena;
    Arena_Init(&arena, "Shot render test", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    memset(g_Video.pages[0], 6, VIDEO_PAGE_SIZE);
    memset(g_Video.pages[1], 12, VIDEO_PAGE_SIZE);
    Video_AccessPage(1);
    Video_SetSplit(123, 456, 789);
    TestClip_Set(0, 0, 639, 399);
    Grcg_Init();

    PATTERN_BANK patterns;
    PatternBank_Init(&patterns, &g_HeapAllocator);
    uint8_t tiny[256];
    uint8_t large[1024];
    memset(tiny, 9, sizeof(tiny));
    tiny[0] = 0;
    memset(large, 5, sizeof(large));
    CHECK(Result_Ignore(PatternBank_Put(&patterns, 52, 16, 16, tiny, 0)));
    CHECK(Result_Ignore(PatternBank_Put(&patterns, 60, 16, 16, tiny, 0)));
    CHECK(Result_Ignore(PatternBank_Put(&patterns, 39, 32, 32, large, 0)));
    CHECK(Result_Ignore(PatternBank_ConvertTiny(&patterns, 52)));
    CHECK(Result_Ignore(PatternBank_ConvertTiny(&patterns, 60)));
    memset(p_Background, 0, sizeof(*p_Background));
    p_Background->cache.mode = TILE_BLACK;
    int16_t scroll = 63;
    Shots_Init();
    for (size_t i = 0; i < 3; i++) {
        m_Shots.slots[i].state = ENTITY_ACTIVE;
        m_Shots.slots[i].position[1] =
            (ENTITY_POINT) { (int16_t)((200 + i * 50) * 16), 336 * 16 };
    }
    m_Shots.slots[0].sprite = 52;
    m_Shots.frames[0] = 1;
    m_Shots.slots[1].sprite = 59;
    m_Shots.slots[1].age = 1;
    m_Shots.slots[1].from_option = 1;
    m_Shots.slots[2].sprite = 124;
    m_Shots.slots[2].age = 1;
    Shots_Update((PLAYFIELD_PAGES) { 1, 0, scroll }, false, 1, &patterns);
    CHECK_EQ_INT(g_Video.pages[1][399 * 640 + 200], 12);
    CHECK_EQ_INT(g_Video.pages[1][399 * 640 + 201], 9);
    CHECK_EQ_INT(g_Video.pages[1][14 * 640 + 215], 9);
    CHECK_EQ_INT(g_Video.pages[1][399 * 640 + 251], 9);
    CHECK_EQ_INT(g_Video.pages[1][14 * 640 + 265], 9);
    CHECK_EQ_INT(g_Video.pages[1][399 * 640 + 300], 5);
    CHECK_EQ_INT(g_Video.pages[1][30 * 640 + 331], 5);
    Shots_Invalidate((PLAYFIELD_PAGES) { 1, 0, scroll }, false);
    Background_RepaintDirty();
    CHECK_EQ_INT(g_Video.pages[1][399 * 640 + 201], 0);
    CHECK_EQ_INT(g_Video.pages[1][14 * 640 + 215], 0);
    CHECK_EQ_INT(g_Video.pages[1][399 * 640 + 251], 0);
    CHECK_EQ_INT(g_Video.pages[1][30 * 640 + 331], 0);
    CHECK_EQ_INT(g_Video.pages[1][100 * 640 + 200], 12);
    for (size_t i = 0; i < VIDEO_PAGE_SIZE; i++) {
        CHECK_EQ_INT(g_Video.pages[0][i], 6);
        if (i >= VIDEO_VISIBLE_SIZE) {
            CHECK_EQ_INT(g_Video.pages[1][i], 12);
        }
    }
    CHECK_EQ_INT(g_Video.drawing_page, 1);
    CHECK_EQ_INT(g_Video.visible_page, 0);
    CHECK_EQ_INT(g_Video.first_lines, 123);
    CHECK_EQ_INT(g_Video.first_address, 456);
    CHECK_EQ_INT(g_Video.second_address, 789);
    CHECK_EQ_INT(g_Grcg->mode, 0);
    PatternBank_Reset(&patterns);
    Arena_Free(&arena);
}
