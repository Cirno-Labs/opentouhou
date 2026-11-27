#include "tests/test.h"
#include "tests/th02/th02_player_fixture.h"
#include "tests/th02/th02_player_graphics.h"
#include "th02/background.h"
#include "th02/background/private.h"
#include "th02/player.h"
#include "th02/shots.h"

static SHOT M_Active(void)
{
    return (
        SHOT) { .state = ENTITY_ACTIVE,
                .position = { { 100 * 16, 100 * 16 }, { 200 * 16, 200 * 16 } },
                .sprite = 52 };
}

TEST(clear_preserves_animation_and_option_state_between_stages)
{
    Shots_Init();
    CHECK_EQ_INT(m_Shots.option_frames_per_age, 4);
    m_Shots.slots[0] = M_Active();
    m_Shots.slots[0].age = 3;
    m_Shots.frames[0] = 255;
    m_Shots.option_active = 7;
    Shots_FreeAll();
    CHECK_EQ_INT(m_Shots.slots[0].state, ENTITY_FREE);
    CHECK_EQ_INT(m_Shots.slots[0].age, 3);
    CHECK_EQ_INT(m_Shots.slots[0].position[0].x, 1600);
    CHECK_EQ_INT(m_Shots.frames[0], 255);
    CHECK_EQ_INT(m_Shots.option_active, 7);
}

TEST(cleanup_marks_old_positions_before_copy_and_clears_last_removed_option)
{
    Shots_Init();
    memset(p_Background, 0, sizeof(*p_Background));
    m_Shots.option_active = 7;
    m_Shots.slots[0] = M_Active();
    m_Shots.slots[0].state = ENTITY_REMOVE;
    m_Shots.slots[0].position[0] = (ENTITY_POINT) { 65 * 16 - 1, -17 };
    m_Shots.slots[0].sprite = 124;
    m_Shots.slots[0].age = 1;
    m_Shots.slots[0].from_option = 1;
    m_Shots.slots[1] = M_Active();
    m_Shots.slots[1].from_option = 1;
    Shots_Invalidate((PLAYFIELD_PAGES) { 0, 1, 0 }, true);
    const TILE_CACHE expected = {
        .columns = { [2] = 1, [3] = 1 },
        .dirty = { [1][24] = 1,
                   [2][0] = 1,
                   [2][1] = 1,
                   [2][24] = 1,
                   [3][0] = 1,
                   [3][1] = 1 },
    };
    CHECK(memcmp(&p_Background->cache, &expected, sizeof(expected)) == 0);
    CHECK_EQ_INT(p_Background->cache.dirty[1][24], 1);
    CHECK_EQ_INT(p_Background->cache.dirty[2][1], 1);
    CHECK_EQ_INT(p_Background->cache.dirty[3][1], 1);
    CHECK_EQ_INT(m_Shots.slots[0].state, ENTITY_FREE);
    CHECK_EQ_INT(m_Shots.slots[0].position[0].x, 65 * 16 - 1);
    CHECK_EQ_INT(m_Shots.slots[1].position[0].x, 3200);
    CHECK_EQ_INT(m_Shots.option_active, 7);
    m_Shots.slots[1].state = ENTITY_REMOVE;
    Shots_Invalidate((PLAYFIELD_PAGES) { 0, 1, 0 }, true);
    CHECK(memcmp(&p_Background->cache, &expected, sizeof(expected)) == 0);
    CHECK_EQ_INT(m_Shots.option_active, 0);
    CHECK_EQ_INT(m_Shots.slots[1].state, ENTITY_FREE);
}

TEST(body_hit_draws_its_last_age_before_delayed_removal)
{
    TEST_PLAYER_GRAPHICS graphics;
    TestPlayerGraphics_Init(&graphics);
    Shots_Init();
    m_Shots.slots[0] = M_Active();
    m_Shots.slots[0].sprite = 124;
    m_Shots.slots[0].age = 5;
    m_Shots.frames[0] = 255;
    Shots_Update((PLAYFIELD_PAGES) { 0, 0, 399 }, false, 4, &graphics.patterns);
    CHECK_EQ_INT(g_Video.pages[0][99 * VIDEO_WIDTH + 100], 3);
    CHECK_EQ_INT(g_Video.pages[0][130 * VIDEO_WIDTH + 131], 3);
    CHECK_EQ_INT(g_Video.pages[0][131 * VIDEO_WIDTH + 132], 0);
    CHECK_EQ_INT(m_Shots.slots[0].age, 6);
    CHECK_EQ_INT(m_Shots.slots[0].state, ENTITY_ACTIVE);
    CHECK_EQ_INT(m_Shots.frames[0], 0);
    memset(g_Video.pages[0], 0, VIDEO_PAGE_SIZE);
    Shots_Update((PLAYFIELD_PAGES) { 0, 0, 0 }, false, 5, &graphics.patterns);
    for (size_t i = 0; i < VIDEO_PAGE_SIZE; i++) {
        CHECK_EQ_INT(g_Video.pages[0][i], 0);
    }
    CHECK_EQ_INT(m_Shots.slots[0].state, ENTITY_REMOVE);
    CHECK_EQ_INT(m_Shots.frames[0], 1);
    CHECK_EQ_INT(m_Shots.slots[0].position[1].x, 3200);
    TestPlayerGraphics_Free(&graphics);
}

TEST(movement_wraps_and_rejects_edges_before_animation)
{
    TEST_PLAYER_GRAPHICS graphics;
    TestPlayerGraphics_Init(&graphics);
    Shots_Init();
    m_Shots.slots[0] = M_Active();
    m_Shots.slots[0].position[0].x = 32767;
    m_Shots.slots[0].velocity.x = 1;
    m_Shots.slots[1] = M_Active();
    m_Shots.slots[1].position[0].x = 256;
    m_Shots.slots[2] = M_Active();
    m_Shots.slots[2].position[0].x = 257;
    Shots_Update((PLAYFIELD_PAGES) { 0, 0, 0 }, false, 0, &graphics.patterns);
    CHECK_EQ_INT(m_Shots.slots[0].position[0].x, -32768);
    CHECK_EQ_INT(m_Shots.slots[0].state, ENTITY_REMOVE);
    CHECK_EQ_INT(m_Shots.slots[1].state, ENTITY_REMOVE);
    CHECK_EQ_INT(m_Shots.slots[2].state, ENTITY_ACTIVE);
    CHECK_EQ_INT(m_Shots.frames[0], 0);
    CHECK_EQ_INT(m_Shots.frames[1], 0);
    CHECK_EQ_INT(m_Shots.frames[2], 1);
    CHECK_EQ_INT(g_Video.pages[0][100 * VIDEO_WIDTH + 16], 9);
    TestPlayerGraphics_Free(&graphics);
}

TEST(reduced_draws_keep_option_motion_and_unsigned_age_wrap)
{
    TEST_PLAYER_GRAPHICS graphics;
    TestPlayerGraphics_Init(&graphics);
    Shots_Init();
    m_Shots.slots[0] = M_Active();
    m_Shots.slots[0].from_option = 1;
    m_Shots.slots[0].age = 2;
    m_Shots.slots[0].sprite = 59;
    m_Shots.slots[0].velocity.y = -3;
    Shots_Update((PLAYFIELD_PAGES) { 1, 0, 0 }, true, 1, &graphics.patterns);
    for (size_t i = 0; i < VIDEO_PAGE_SIZE; i++) {
        CHECK_EQ_INT(g_Video.pages[0][i], 0);
        CHECK_EQ_INT(g_Video.pages[1][i], 0);
    }
    CHECK_EQ_INT(m_Shots.slots[0].position[1].y, 3197);
    CHECK_EQ_INT(m_Shots.slots[0].velocity.y, -2);
    m_Shots.option_frames_per_age = -1;
    m_Shots.slots[0].age = 255;
    Shots_Update(
        (PLAYFIELD_PAGES) { 0, 0, 0 }, false, UINT32_MAX, &graphics.patterns);
    CHECK_EQ_INT(m_Shots.slots[0].age, 0);
    CHECK_EQ_INT(g_Video.pages[0][99 * VIDEO_WIDTH + 100], 15);
    CHECK_EQ_INT(m_Shots.slots[0].state, ENTITY_ACTIVE);
    TestPlayerGraphics_Free(&graphics);
}
