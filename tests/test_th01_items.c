#include "th01/items.h"
#include "th01/shots.h"
#include "th01/rng.h"
#include "th01/player.h"
#include "th01/pellets.h"
#include "th01/orb.h"

#include "core/base/arena.h"

#include "th01_video.h"
#include "th01_world.h"
#include "test.h"

#include <string.h>

typedef struct {
    ARENA arena;
    ITEM_CONTEXT context;
    PTN sprites;
    GAME_TEXT_FONT font;
    uint16_t value;
    int32_t events[64], count;
    uint8_t callback_flags[64][3];
} CASE;

static void M_Record(CASE *const c, const int32_t event)
{
    c->callback_flags[c->count][0] = g_Items.bombs[2].flag;
    c->callback_flags[c->count][1] = g_Items.bombs[3].flag;
    c->callback_flags[c->count][2] = g_Items.points[0].flag;
    c->events[c->count++] = event;
}

static void M_HudBomb(void *const user, const int16_t slot)
{
    CASE *const c = user;
    CHECK_EQ_INT(slot, g_Player.remaining_bombs - 1);
    M_Record(c, 1);
}

static void M_HudScore(void *const user)
{
    CASE *const c = user;
    M_Record(c, 2);
}

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
    c->context = (ITEM_CONTEXT) {
        .point_value = &c->value,
        .sprites = &c->sprites,
        .font = &c->font,
        .hud_bomb = M_HudBomb,
        .hud_score_and_combo = M_HudScore,
        .user = c,
    };
}

TEST(pool_splash_fall_bounce_and_reset_boundaries)
{
    CASE c;
    M_Init(&c);
    for (int32_t i = 0; i < 5; i++) {
        Items_Add((int16_t)(100 + i), 366, true);
    }
    CHECK_EQ_INT(g_Items.bombs[3].left, 103);
    for (int32_t i = 0; i < 11; i++) {
        Items_UpdateAndRender(&c.context);
    }
    CHECK_EQ_INT(g_Items.bombs[0].flag, ITEM_FALL);
    CHECK_EQ_INT(g_Items.bombs[0].top, 366);
    CHECK_EQ_INT(Th01Test_Pixel(0, 100, 366), 0);
    Items_UpdateAndRender(&c.context);
    CHECK_EQ_INT(g_Items.bombs[0].flag, ITEM_BOUNCE);
    CHECK_EQ_INT(g_Items.bombs[0].top, 360);
    CHECK_EQ_INT(g_Items.bombs[0].velocity_y, -7);
    Items_Reset();
    CHECK_EQ_INT(g_Items.bombs[0].flag, ITEM_FREE);
    CHECK_EQ_INT(g_Items.bombs[0].top, 360);
    CHECK_EQ_INT(g_Items.bombs[0].velocity_y, -7);
    Items_Add(200, 123, true);
    CHECK_EQ_INT(g_Items.bombs[0].left, 200);
    Arena_Free(&c.arena);
}

TEST(strict_collision_and_immediate_popup_with_bomb_cap)
{
    CASE c;
    M_Init(&c);
    g_Player.left = 100;
    g_Player.remaining_bombs = 4;
    g_Items.bombs[0] = (ITEM) { .left = 76, .top = 345, .flag = ITEM_FALL };
    g_Items.bombs[1] = (ITEM) { .left = 100, .top = 344, .flag = ITEM_FALL };
    g_Items.bombs[2] = (ITEM) { .left = 123, .top = 1000, .flag = ITEM_FALL };
    g_Items.bombs[3] = (ITEM) { .left = 100, .top = 345, .flag = ITEM_BOUNCE };
    Items_UpdateAndRender(&c.context);
    CHECK_EQ_INT(g_Items.bombs[0].flag, ITEM_FALL);
    CHECK_EQ_INT(g_Items.bombs[1].flag, ITEM_FALL);
    CHECK_EQ_INT(g_Items.bombs[2].flag, ITEM_COLLECTED);
    CHECK_EQ_INT(g_Items.bombs[2].top, 366);
    CHECK_EQ_INT(g_Items.bombs[2].radius_or_time, 15);
    CHECK_EQ_INT(g_Items.bombs[3].flag, ITEM_COLLECTED_OVER_CAP);
    CHECK_EQ_INT(g_Player.remaining_bombs, 5);
    CHECK_EQ_INT(g_Stage.score, 10000);
    CHECK_EQ_INT(c.count, 2);
    CHECK_EQ_INT(c.events[0], 1);
    CHECK_EQ_INT(c.events[1], 2);
    CHECK_EQ_INT(c.callback_flags[0][0], ITEM_FALL);
    CHECK_EQ_INT(c.callback_flags[1][0], ITEM_COLLECTED);
    CHECK_EQ_INT(c.callback_flags[1][1], ITEM_BOUNCE);
    for (int32_t i = 0; i < 15; i++) {
        Items_UpdateAndRender(&c.context);
    }
    CHECK_EQ_INT(g_Items.bombs[2].flag, ITEM_FREE);
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
        Items_Reset();
        c.value = before[i];
        g_Items.points[0] = (ITEM) { .left = 0, .top = 345, .flag = ITEM_FALL };
        const uint32_t score = g_Stage.score;
        Items_UpdateAndRender(&c.context);
        CHECK_EQ_INT(c.value, after[i]);
        CHECK_EQ_INT(g_Stage.score, score + after[i]);
        CHECK_EQ_INT(c.callback_flags[c.count - 1][2], ITEM_FALL);
    }
    Items_Reset();
    c.value = 10000;
    g_Items.points[0] = (ITEM) {
        .left = 100, .top = 398, .velocity_y = 2, .flag = ITEM_BOUNCE
    };
    g_Items.points[1] = (ITEM) { .left = 200,
                                 .top = 200,
                                 .velocity_y = -2,
                                 .flag = ITEM_COLLECTED_OVER_CAP,
                                 .radius_or_time = 3 };
    Items_UpdateAndRender(&c.context);
    CHECK_EQ_INT(c.value, 0);
    CHECK_EQ_INT(g_Items.points[0].flag, ITEM_FREE);
    CHECK_EQ_INT(g_Items.points[1].radius_or_time, 2);
    CHECK_EQ_INT(Th01Test_Pixel(0, 200 + 4 * 8 + 2, 198), 7);
    CHECK_EQ_INT(Th01Test_Pixel(0, 200 + 4 * 8 + 3, 198), 0);
    Arena_Free(&c.arena);
}
