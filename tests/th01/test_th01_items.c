#include "tests/test.h"
#include "tests/th01/th01_text_fixture.h"
#include "tests/th01/th01_video.h"
#include "tests/th01/th01_world.h"
#include "th01/hud/hud.c"
#include "th01/items.c"
#include "th01/items.h"
#include "th01/orb.h"
#include "th01/pellets.h"
#include "th01/player_private.h"
#include "th01/rng.h"
#include "th01/session_private.h"
#include "th01/shots.h"

#include "core/base/arena.h"

#include <string.h>

typedef struct {
    ARENA arena;
    PTN sprites;
    GAME_TEXT_FONT font;
} CASE;

static void M_Init(CASE *const c)
{
    *c = (CASE) {};
    Th01Test_ResetWorld();
    Arena_Init(&c->arena, "items", 1024 * 1024);
    Video_Init(Arena_Allocator(&c->arena));
    c->sprites =
        (PTN) { .count = 34, .pixels = Arena_Alloc(&c->arena, 34 * 1024) };
    memset(c->sprites.pixels, 5, 34 * 1024);
    GAME_TEXT_GLYPH glyphs[97] = {};
    for (int32_t i = 0; i < 95; i++) {
        glyphs[i] =
            (GAME_TEXT_GLYPH) { .codepoint = (uint32_t)(i + 32), .width = 8 };
        // Distinguish changing popup digits without depending on a ROM font.
        glyphs[i].rows[0] = (uint16_t)((i + 32) << 8);
    }
    glyphs[95] = (GAME_TEXT_GLYPH) { .codepoint = 0xff11, .width = 16 };
    glyphs[96] = (GAME_TEXT_GLYPH) { .codepoint = 0x842c, .width = 16 };
    CHECK(Result_Ignore(
        GameText_LoadGlyphs(glyphs, 97, Arena_Allocator(&c->arena), &c->font)));
    CHECK(Result_Ignore(Items_ValidateAssets(&c->sprites, &c->font)));
    Hud_Init(Arena_Allocator(&c->arena));
    Hud_Load(nullptr, &c->sprites, &c->font);
}

TEST(pool_splash_fall_bounce_and_reset_boundaries)
{
    CASE c;
    M_Init(&c);
    for (int32_t i = 0; i < 5; i++) {
        Items_Add((int16_t)(100 + i), 366, true);
    }
    CHECK_EQ_INT(m_Items.bombs[3].left, 103);
    for (int32_t i = 0; i < 11; i++) {
        Items_Update(&c.sprites, &c.font);
    }
    CHECK_EQ_INT(m_Items.bombs[0].flag, ITEM_FALL);
    CHECK_EQ_INT(m_Items.bombs[0].top, 366);
    CHECK_EQ_INT(Th01Test_Pixel(0, 100, 366), 0);
    Items_Update(&c.sprites, &c.font);
    CHECK_EQ_INT(m_Items.bombs[0].flag, ITEM_BOUNCE);
    CHECK_EQ_INT(m_Items.bombs[0].top, 360);
    CHECK_EQ_INT(m_Items.bombs[0].velocity_y, -7);
    Items_ResetStage();
    CHECK_EQ_INT(m_Items.bombs[0].flag, ITEM_FREE);
    CHECK_EQ_INT(m_Items.bombs[0].top, 360);
    CHECK_EQ_INT(m_Items.bombs[0].velocity_y, -7);
    Items_Add(200, 123, true);
    CHECK_EQ_INT(m_Items.bombs[0].left, 200);
    Arena_Free(&c.arena);
}

TEST(strict_collision_and_immediate_popup_with_bomb_cap)
{
    CASE c;
    M_Init(&c);
    p_Player->left = 100;
    p_Player->remaining_bombs = 4;
    m_Items.bombs[0] = (ITEM) { .left = 76, .top = 345, .flag = ITEM_FALL };
    m_Items.bombs[1] = (ITEM) { .left = 100, .top = 344, .flag = ITEM_FALL };
    m_Items.bombs[2] = (ITEM) { .left = 123, .top = 1000, .flag = ITEM_FALL };
    m_Items.bombs[3] = (ITEM) { .left = 100, .top = 345, .flag = ITEM_BOUNCE };
    Items_Update(&c.sprites, &c.font);
    CHECK_EQ_INT(m_Items.bombs[0].flag, ITEM_FALL);
    CHECK_EQ_INT(m_Items.bombs[1].flag, ITEM_FALL);
    CHECK_EQ_INT(m_Items.bombs[2].flag, ITEM_COLLECTED);
    CHECK_EQ_INT(m_Items.bombs[2].top, 366);
    CHECK_EQ_INT(m_Items.bombs[2].radius_or_time, 15);
    CHECK_EQ_INT(m_Items.bombs[3].flag, ITEM_COLLECTED_OVER_CAP);
    CHECK_EQ_INT(p_Player->remaining_bombs, 5);
    CHECK_EQ_INT(g_Stage->score, 10000);
    CHECK_EQ_INT(m_Hud.score_previous, 10000);
    for (int32_t i = 0; i < 15; i++) {
        Items_Update(&c.sprites, &c.font);
    }
    CHECK_EQ_INT(m_Items.bombs[2].flag, ITEM_FREE);
    Arena_Free(&c.arena);
}

TEST(point_width_wrap_cap_and_live_popup_on_drop)
{
    static const uint16_t before[] = { 0,     9000,  10000, 49999, 50000, 55999,
                                       56000, 59998, 59999, 60000, 65535 };
    static const uint16_t after[] = { 1000, 10000, 20000, 59999, 65530, 463,
                                      464,  4462,  59999, 65530, 65530 };
    CASE c;
    M_Init(&c);
    for (size_t i = 0; i < sizeof(before) / sizeof(*before); i++) {
        Items_ResetStage();
        p_Session->point_value = before[i];
        m_Items.points[0] = (ITEM) { .left = 0, .top = 345, .flag = ITEM_FALL };
        const uint32_t score = g_Stage->score;
        Items_Update(&c.sprites, &c.font);
        CHECK_EQ_INT(p_Session->point_value, after[i]);
        CHECK_EQ_INT(g_Stage->score, score + after[i]);
        CHECK_EQ_INT(m_Hud.score_previous, g_Stage->score);
    }
    Items_ResetStage();
    p_Session->point_value = 10000;
    m_Items.points[0] = (ITEM) {
        .left = 100, .top = 398, .velocity_y = 2, .flag = ITEM_BOUNCE
    };
    m_Items.points[1] = (ITEM) { .left = 200,
                                 .top = 200,
                                 .velocity_y = -2,
                                 .flag = ITEM_COLLECTED_OVER_CAP,
                                 .radius_or_time = 3 };
    Items_Update(&c.sprites, &c.font);
    CHECK_EQ_INT(p_Session->point_value, 0);
    CHECK_EQ_INT(m_Items.points[0].flag, ITEM_FREE);
    CHECK_EQ_INT(m_Items.points[1].radius_or_time, 2);
    CHECK_EQ_INT(Th01Test_Pixel(0, 200 + 4 * 8 + 2, 198), 7);
    CHECK_EQ_INT(Th01Test_Pixel(0, 200 + 4 * 8 + 3, 198), 0);
    Arena_Free(&c.arena);
}
