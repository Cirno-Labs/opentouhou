#include "tests/test.h"
#include "tests/th02/th02_player_fixture.h"
#include "tests/th02/th02_player_graphics.h"
#include "tests/th02/th02_random_fixture.h"
#include "tests/th02/th02_score_fixture.h"
#include "tests/th02/th02_sound.h"
#include "tests/th02/th02_tram_fixture.h"
#include "th02/items.h"
#include "th02/miss.h"
#include "th02/performance_private.h"
#include "th02/player.h"
#include "th02/shots.h"
#include "th02/sparks.h"

#include <string.h>

typedef struct {
    uint8_t start_bombs;
    TEST_PLAYER_GRAPHICS graphics;
    int16_t scroll;
} FIXTURE;

static void M_Init(FIXTURE *const fixture)
{
    Sparks_Init();
    Items_Init();
    memset(p_Performance, 0, sizeof(*p_Performance));
    Tram_Init();
    memset(fixture, 0, sizeof(*fixture));
    TestPlayerGraphics_Init(&fixture->graphics);
    Player_Init();
    Player_ResetPosition();
    Random_Reset();
    memset(m_Rng.ring, 1, sizeof(m_Rng.ring));
    m_Player.current = (ENTITY_POINT) { 123, 456 };
    m_Player.hit = 1;
    m_Shots.weapon.level = 9;
    m_Player.power = 80;
    Score_Init();
    m_Score.lives = 2;
    m_Score.bombs = 4;
    m_Score.value = 100;
    m_Score.delta = 77;
    fixture->start_bombs = 1;
    p_Performance->value = 7;
}

TEST(miss_keeps_score_display_and_current_position_lag_through_respawn)
{
    FIXTURE fixture;
    M_Init(&fixture);
    TEST_SOUND sound;
    TestSound_Init(&sound);
    fixture.scroll = 63;
    memset(&m_Tram, 0xa5, sizeof(m_Tram));
    CHECK(!Player_Update(
        (PLAYFIELD_PAGES) { 0, 0, fixture.scroll }, 0, false,
        &fixture.graphics.patterns, fixture.start_bombs));
    CHECK_EQ_INT(m_Score.value, 177);
    CHECK_EQ_INT(m_Score.delta, 0);
    CHECK_EQ_INT(m_Tram.codes[4 * 80 + 56], 0xa5a5);
    CHECK_EQ_INT(p_Performance->total_misses, 1);
    CHECK_EQ_INT(m_Miss.stage_misses, 1);
    CHECK_EQ_INT(m_Miss.active, 1);
    TestSound_Check(&sound, -1);
    SoundEffects_Update();
    TestSound_Check(&sound, 2);
    CHECK_EQ_INT(g_Sparks->next, 16);
    CHECK_EQ_INT(g_Sparks->slots[0].position[0].x, 224 * 16);
    CHECK_EQ_INT(g_Sparks->slots[0].position[0].y, 360 * 16);
    CHECK_EQ_INT(g_Sparks->slots[0].position[1].x, 0);
    CHECK_EQ_INT(g_Sparks->slots[0].velocity.x, 128);
    CHECK_EQ_INT(g_Sparks->slots[0].drawing, 14);
    CHECK_EQ_INT(m_Rng.cursor, 0);
    CHECK_EQ_INT(g_Video.pages[0][399 * VIDEO_WIDTH + 208], 5);
    for (size_t frame = 2; frame <= 24; frame++) {
        CHECK(!Player_Update(
            (PLAYFIELD_PAGES) { 0, 0, fixture.scroll }, 0, false,
            &fixture.graphics.patterns, fixture.start_bombs));
    }
    CHECK_EQ_INT(m_Player.invincibility, 220);
    CHECK_EQ_INT(m_Player.power, 64);
    CHECK_EQ_INT(m_Shots.weapon.level, 8);
    CHECK_EQ_INT(p_Performance->value, 0);
    CHECK_EQ_INT(g_Sparks->next, 48);
    CHECK_EQ_INT(g_Sparks->slots[16].position[0].y, 360 * 16);
    for (size_t i = 0; i < 5; i++) {
        const ITEM *const item = &g_Items->slots[i];
        CHECK_EQ_INT(item->state, ENTITY_ACTIVE);
        CHECK_EQ_INT(item->kind, i == 4 ? ITEM_BIG_POWER : ITEM_POINT);
        CHECK_EQ_INT(item->position[0].x, 208);
        CHECK_EQ_INT(item->position[0].y, 336 * 16);
        CHECK_EQ_INT(item->position[1].y, 336 * 16);
        CHECK_EQ_INT(item->drift_x, (int)i - 2);
    }
    CHECK_EQ_INT(g_Items->slots[5].state, ENTITY_FREE);
    CHECK_EQ_INT(m_Rng.cursor, 9);
    for (size_t frame = 25; frame <= 42; frame++) {
        CHECK(!Player_Update(
            (PLAYFIELD_PAGES) { 0, 0, fixture.scroll }, 0, false,
            &fixture.graphics.patterns, fixture.start_bombs));
    }
    CHECK_EQ_INT(m_Player.position[0].y, 408);
    CHECK_EQ_INT(m_Player.current.y, 404);
    CHECK(!Player_Update(
        (PLAYFIELD_PAGES) { 0, 0, fixture.scroll }, 0, false,
        &fixture.graphics.patterns, fixture.start_bombs));
    CHECK_EQ_INT(g_Sparks->next, 0);
    CHECK_EQ_INT(g_Sparks->slots[48].position[0].y, 32 * 16);
    CHECK_EQ_INT(m_Score.lives, 1);
    CHECK_EQ_INT(m_Score.bombs, 1);
    CHECK_EQ_INT(m_Player.current.y, 408);
    CHECK_EQ_INT(m_Player.position[0].y, 366);
    CHECK_EQ_INT(m_Player.position[1].y, 366);
    CHECK(!Player_Update(
        (PLAYFIELD_PAGES) { 0, 0, fixture.scroll }, 0, false,
        &fixture.graphics.patterns, fixture.start_bombs));
    CHECK_EQ_INT(m_Player.current.y, 366);
    CHECK_EQ_INT(m_Player.position[0].y, 364);
    CHECK_EQ_INT(g_Video.pages[0][27 * VIDEO_WIDTH + 208], 1);
    for (size_t frame = 45; frame <= 68; frame++) {
        CHECK(!Player_Update(
            (PLAYFIELD_PAGES) { 0, 0, fixture.scroll }, 0, false,
            &fixture.graphics.patterns, fixture.start_bombs));
    }
    CHECK_EQ_INT(m_Miss.frame, 0);
    CHECK_EQ_INT(m_Miss.active, 0);
    CHECK_EQ_INT(m_Player.hit, 0);
    CHECK_EQ_INT(m_Player.invincibility, 176);
    TestSound_Free(&sound);
    TestPlayerGraphics_Free(&fixture.graphics);
}

TEST(gameover_drops_use_the_flag_without_rng_and_player_update_consumes_quit)
{
    FIXTURE fixture;
    M_Init(&fixture);
    m_Miss.frame = 23;
    m_Miss.active = 1;
    m_Score.lives = 0;
    p_Performance->value = -5;
    const RANDOM_STATE before_random = m_Rng;
    CHECK(Player_Update(
        (PLAYFIELD_PAGES) { 0, 0, 0 }, 0, false, &fixture.graphics.patterns,
        fixture.start_bombs));
    CHECK_EQ_INT(p_Performance->value, -6);
    CHECK_EQ_INT(m_Miss.frame, 0);
    CHECK_EQ_INT(m_Miss.item_gameover, 0);
    CHECK_EQ_INT(m_Miss.active, 1);
    CHECK_EQ_INT(m_Player.hit, 0);
    CHECK_EQ_INT(m_Player.power, 80);
    CHECK_EQ_INT(g_Sparks->next, 32);
    CHECK_EQ_INT(g_Sparks->slots[0].position[0].y, 360 * 16);
    for (size_t i = 0; i < 5; i++) {
        CHECK_EQ_INT(g_Items->slots[i].state, ENTITY_ACTIVE);
        CHECK_EQ_INT(g_Items->slots[i].kind, ITEM_BIG_POWER);
    }
    CHECK_EQ_INT(g_Items->slots[5].state, ENTITY_FREE);
    CHECK(memcmp(&m_Rng, &before_random, sizeof(before_random)) == 0);
    TestPlayerGraphics_Free(&fixture.graphics);
}

TEST(miss_byte_overflow_and_last_life_bomb_wrap_preserve_partial_state)
{
    FIXTURE fixture;
    M_Init(&fixture);
    m_Miss.frame = 255;
    Miss_Update(
        (PLAYFIELD_PAGES) { 1, 0, 0 }, &fixture.graphics.patterns,
        fixture.start_bombs);
    CHECK_EQ_INT(m_Miss.frame, 0);
    CHECK_EQ_INT(m_Miss.stage_misses, 0);
    CHECK_EQ_INT(m_Score.delta, 77);
    CHECK_EQ_INT(g_Sparks->next, 0);
    CHECK_EQ_INT(g_Items->slots[0].state, ENTITY_FREE);
    CHECK_EQ_INT(g_Video.pages[0][336 * VIDEO_WIDTH + 208], 5);
    CHECK_EQ_INT(g_Video.pages[1][336 * VIDEO_WIDTH + 208], 0);
    CHECK_EQ_INT(g_Video.drawing_page, 0);
    m_Miss.frame = 42;
    m_Score.lives = 1;
    fixture.start_bombs = 255;
    Miss_Update(
        (PLAYFIELD_PAGES) { 1, 0, 0 }, &fixture.graphics.patterns,
        fixture.start_bombs);
    CHECK_EQ_INT(m_Score.lives, 0);
    CHECK_EQ_INT(m_Score.bombs, 1);
    CHECK_EQ_INT(g_Sparks->next, 16);
    CHECK_EQ_INT(g_Sparks->slots[0].position[1].y, 360 * 16);
    CHECK_EQ_INT(g_Sparks->slots[0].position[0].y, 0);
    TestPlayerGraphics_Free(&fixture.graphics);
}
