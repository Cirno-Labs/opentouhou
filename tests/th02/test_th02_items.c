#include "tests/test.h"
#include "tests/th02/th02_player_fixture.h"
#include "tests/th02/th02_player_graphics.h"
#include "tests/th02/th02_random_fixture.h"
#include "tests/th02/th02_score_fixture.h"
#include "tests/th02/th02_sound.h"
#include "th02/background.h"
#include "th02/items_private.h"
#include "th02/performance_private.h"
#include "th02/player.h"
#include "th02/point_numbers_private.h"

#include <string.h>

typedef struct {
    TEST_PLAYER_GRAPHICS graphics;
    POINT_NUMBER_FONT font;
} FIXTURE;

static void M_Init(FIXTURE *const fixture)
{
    memset(fixture, 0, sizeof(*fixture));
    TestPlayerGraphics_Init(&fixture->graphics);
    Items_Init();
    PointNumbers_Init();
    PointNumbers_ResetStage(4);
    Score_Init();
    memset(p_Performance, 0, sizeof(*p_Performance));
    Tram_Init();
    memset(p_Rng, 0, sizeof(*p_Rng));
    Player_Init();
}

static void M_Free(FIXTURE *const fixture)
{
    TestPlayerGraphics_Free(&fixture->graphics);
}

TEST(full_bank_consumes_overrides_and_wrapped_cycle_keeps_consecutive_attempts)
{
    FIXTURE fixture;
    memset(p_Items, 0xa5, sizeof(*p_Items));
    M_Init(&fixture);
    CHECK_EQ_INT(p_Items->big_power, 0);
    CHECK_EQ_INT(p_Items->point_count, 0);
    CHECK_EQ_INT(p_Items->frame, 0);
    CHECK_EQ_INT(p_Items->overflow, 0);
    CHECK_EQ_INT(p_Items->ring_cursor, 0);
    CHECK_EQ_INT(p_Items->cycle, 0);
    CHECK_EQ_INT(p_Items->frame_score, 0);
    CHECK_EQ_INT(p_Items->current.x, 0);
    CHECK_EQ_INT(p_Items->slots[ITEM_COUNT - 1].state, 0);
    for (size_t i = 0; i < ITEM_COUNT; i++) {
        p_Items->slots[i].state = ENTITY_ACTIVE;
    }
    p_Items->big_power = 2;
    p_Items->cycle = 99;
    Items_Add(200, 300, ITEM_POWER);
    CHECK_EQ_INT(p_Items->big_power, 1);
    Items_AddRandom(200, 300);
    CHECK_EQ_INT(p_Items->big_power, 0);
    CHECK_EQ_INT(p_Items->cycle, 99);
    CHECK_EQ_INT(p_Rng->cursor, 0);
    p_Items->frame = 123;
    p_Items->overflow = 17;
    p_Items->slots[0].age = 23;
    p_Items->slots[0].speed_y = 99;
    p_Miss->item_gameover = 7;
    p_PointNumbers->slots[0].state = ENTITY_ACTIVE;
    p_PointNumbers->slots[0].age = 23;
    Items_ResetStage(2);
    CHECK_EQ_INT(p_Items->slots[0].state, ENTITY_FREE);
    CHECK_EQ_INT(p_PointNumbers->slots[0].state, ENTITY_FREE);
    CHECK_EQ_INT(p_PointNumbers->slots[0].age, 23);
    CHECK_EQ_INT(p_PointNumbers->operation, 10);
    CHECK_EQ_INT(p_PointNumbers->operand, 2);
    CHECK_EQ_INT(p_Miss->item_gameover, 0);
    CHECK_EQ_INT(p_Items->frame, 123);
    CHECK_EQ_INT(p_Items->overflow, 17);
    CHECK_EQ_INT(p_Items->cycle, 99);
    CHECK_EQ_INT(p_Items->slots[0].age, 23);
    CHECK_EQ_INT(p_Items->slots[0].speed_y, 99);
    p_Items->cycle = 255;
    p_Rng->cursor = 255;
    p_Rng->ring[255] = 18;
    Items_AddRandom(200, 300);
    CHECK_EQ_INT(p_Items->slots[0].kind, ITEM_BOMB);
    CHECK_EQ_INT(p_Items->cycle, 0);
    CHECK_EQ_INT(p_Rng->cursor, 0);
    Items_AddRandom(200, 300);
    CHECK_EQ_INT(p_Items->slots[1].kind, ITEM_POINT);
    CHECK_EQ_INT(p_Items->cycle, 1);
    CHECK_EQ_INT(p_Rng->cursor, 1);
    M_Free(&fixture);
}

TEST(partial_recovery_uses_player_zone_and_gameover_drops_do_not_consume_rng)
{
    FIXTURE fixture;
    M_Init(&fixture);
    for (size_t i = 1; i < ITEM_COUNT; i++) {
        p_Items->slots[i].state = ENTITY_REMOVE;
    }
    p_Player->current.x = 128;
    p_Rng->ring[0] = 1;
    Items_Drop(300, 320);
    CHECK_EQ_INT(p_Items->slots[0].kind, ITEM_POWER);
    CHECK_EQ_INT(p_Items->slots[0].drift_x, 2);
    CHECK_EQ_INT(p_Items->slots[0].speed_y, -80);
    CHECK_EQ_INT(p_Items->slots[0].position[0].x, 300);
    CHECK_EQ_INT(p_Rng->cursor, 2);
    for (size_t i = 0; i < ITEM_COUNT; i++) {
        p_Items->slots[i].state = ENTITY_FREE;
    }
    p_Miss->item_gameover = 1;
    Items_Drop(300, 320);
    CHECK_EQ_INT(p_Rng->cursor, 2);
    for (size_t i = 0; i < 5; i++) {
        CHECK_EQ_INT(p_Items->slots[i].kind, ITEM_BIG_POWER);
        CHECK_EQ_INT(p_Items->slots[i].state, ENTITY_ACTIVE);
    }
    CHECK_EQ_INT(p_Items->slots[5].state, ENTITY_FREE);
    M_Free(&fixture);
}

TEST(capped_bomb_rewards_precede_removal_effect_skill_commit_and_ranked_delta)
{
    FIXTURE fixture;
    M_Init(&fixture);
    TEST_SOUND sound;
    TestSound_Init(&sound);
    p_Items->slots[0] = (ITEM) {
        .state = ENTITY_ACTIVE,
        .kind = ITEM_BOMB,
        .position = { { 200, 1600 }, { 200, 1600 } },
    };
    p_Player->current = (ENTITY_POINT) { 200, 100 };
    p_Score->bombs = 5;
    p_Score->delta = 10;
    p_Performance->collected = 16;
    p_Performance->item_skill = INT16_MAX;
    Items_Update(
        (PLAYFIELD_PAGES) { 0, 0, 0 }, 4, &fixture.graphics.patterns,
        &fixture.font);
    CHECK_EQ_INT(p_PointNumbers->slots[0].state, ENTITY_ACTIVE);
    CHECK_EQ_INT(p_PointNumbers->slots[0].value, 6553);
    CHECK_EQ_INT(p_PointNumbers->slots[0].left, 188);
    CHECK_EQ_INT(p_PointNumbers->slots[0].top[0], 100);
    CHECK_EQ_INT(p_PointNumbers->slots[0].age, 1);
    CHECK_EQ_INT(p_Items->slots[0].state, ENTITY_REMOVE);
    TestSound_Check(&sound, -1);
    SoundEffects_Update();
    TestSound_Check(&sound, 13);
    CHECK_EQ_INT(p_Performance->collected, 0);
    CHECK_EQ_INT(p_Performance->item_skill, INT16_MIN);
    CHECK_EQ_INT(p_Score->delta, 10 + 6553 * 8);
    TestSound_Free(&sound);
    M_Free(&fixture);
}

TEST(cleanup_marks_old_pixels_then_copies_front_position_before_freeing)
{
    FIXTURE fixture;
    M_Init(&fixture);
    p_Items->slots[0] = (ITEM) {
        .state = ENTITY_REMOVE,
        .position = { { 208, -1 }, { 300, 5120 } },
    };
    PointNumbers_Add(96, 48, 123);
    p_PointNumbers->slots[0].top[1] = 64;
    Items_Invalidate((PLAYFIELD_PAGES) { 0, 1, 0 });
    const TILE_CACHE expected = {
        .columns = { [4] = 1, [5] = 1, [6] = 1, [7] = 1, [11] = 1 },
        .dirty = { [4][3] = 1,
                   [5][3] = 1,
                   [6][3] = 1,
                   [7][3] = 1,
                   [10][24] = 1,
                   [11][0] = 1 },
    };
    CHECK(memcmp(&g_Background->cache, &expected, sizeof(expected)) == 0);
    CHECK_EQ_INT(p_Items->slots[0].state, ENTITY_FREE);
    CHECK_EQ_INT(p_Items->slots[0].position[0].x, 300);
    CHECK_EQ_INT(p_Items->slots[0].position[0].y, 5120);
    CHECK_EQ_INT(p_PointNumbers->slots[0].top[0], 64);
    M_Free(&fixture);
}
