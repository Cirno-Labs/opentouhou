#include "th01/route_select.h"
#include "th01/shots.h"
#include "th01/rng.h"
#include "th01/player.h"
#include "th01/pellets.h"
#include "th01/orb.h"

#include "core/base/arena.h"

#include "th01_world.h"
#include "test.h"

typedef struct {
    GAME_TEXT_FONT font;
    PTN stage;
    SCENE_DATA cache[3];
    ROUTE_SELECT_ASSETS assets;
} FIXTURE;

static void M_Init(FIXTURE *const f, ARENA *const arena)
{
    *f = (FIXTURE) {};
    Th01Test_ResetWorld();
    GameInput_Init();
    Video_Init(Arena_Allocator(arena));
    memset(g_Video.pages[0], 4, VIDEO_PAGE_SIZE);
    memset(g_Video.pages[1], 6, VIDEO_PAGE_SIZE);
    Palette_SetGray(&g_Palette, 3);
    GAME_TEXT_GLYPH glyphs[96] = {};
    for (uint32_t i = 0; i < 90; i++) {
        glyphs[i].codepoint = 0xff01 + i;
    }
    const uint32_t others[] = {
        0x3000, 0x9b54, 0x754c, 0x3078, 0x5730, 0x7344
    };
    for (int32_t i = 0; i < 6; i++) {
        glyphs[90 + i].codepoint = others[i];
    }
    for (int32_t i = 0; i < 96; i++) {
        GAME_TEXT_GLYPH *const glyph = &glyphs[i];
        glyph->width = 16;
        for (uint32_t y = 0; y < 16; y++) {
            glyph->rows[y] = glyph->codepoint == 0x3000
                ? 0
                : (uint16_t)((glyph->codepoint * 73 + y * 317)
                             ^ (glyph->codepoint >> (y % 3)));
        }
    }
    CHECK(Result_Ignore(
        GameText_LoadGlyphs(glyphs, 96, Arena_Allocator(arena), &f->font)));
    f->stage = (PTN) { .count = 4, .pixels = Arena_Alloc(arena, 4096) };
    memset(f->stage.pixels, PTN_TRANSPARENT, 4096);
    f->stage.pixels[3 * 1024] = 7;
    f->stage.pixels[3 * 1024 + 1] = 3;
    for (uint8_t id = 0; id < 3; id++) {
        uint8_t bytes[22 + 5 * 250] = {};
        bytes[22] = (uint8_t)(8 >> id);
        READER reader;
        Reader_OpenBuffer(&reader, bytes, sizeof(bytes), nullptr);
        CHECK(Result_Ignore(Scene_LoadReader(
            &reader, id, Arena_Allocator(arena), &f->cache[id])));
        Reader_Close(&reader);
    }
    Scene_Activate(&f->cache[0]);
    g_Session.route = 1;
    f->assets = (ROUTE_SELECT_ASSETS) { &f->font, &f->stage, &f->cache[1],
                                        &f->cache[2] };
    CHECK(Result_Ignore(RouteSelect_ValidateAssets(&f->assets)));
}

static bool M_Poll(
    ROUTE_SELECT *const selection, FIXTURE *const f, const TH01_INPUT raw)
{
    for (int32_t i = 0; i < 5; i++) {
        Frame_VSync();
    }
    return RouteSelect_Resume(selection, &f->assets, &raw);
}

TEST(held_confirmation_waits_then_activates_real_cached_scene)
{
    ARENA arena;
    Arena_Init(&arena, "route", 2 * 1024 * 1024);
    FIXTURE f;
    M_Init(&f, &arena);
    g_GameInput.shot = g_GameInput.ok = g_GameInput.bomb = true;
    g_GameInput.bomb_frame = 72;
    Frame_Reset();
    ROUTE_SELECT selection = {};
    const TH01_INPUT raw = { .shot = true };
    CHECK(!RouteSelect_Begin(&selection, &f.assets, &raw));
    CHECK(!g_GameInput.shot && !g_GameInput.ok && !g_GameInput.bomb);
    CHECK_EQ_INT(g_GameInput.bomb_frame, 72);
    CHECK_EQ_INT(g_Video.pages[0][250 * VIDEO_WIDTH + 128], 7);
    CHECK_EQ_INT(g_Video.pages[1][250 * VIDEO_WIDTH + 128], 4);
    CHECK_EQ_INT(g_Palette.saved.colors[1][0], 15);
    CHECK_EQ_INT(g_Palette.saved.colors[3][0], 9);
    CHECK_EQ_INT(g_Palette.saved.colors[4][0], 0);
    CHECK_EQ_INT(g_Session.route, 1);
    CHECK(g_Scene.active == &f.cache[0]);
    const ROUTE_SELECT frozen = selection;
    CHECK(!RouteSelect_Resume(&selection, &f.assets, &raw));
    CHECK(memcmp(&frozen, &selection, sizeof(frozen)) == 0);
    for (int32_t i = 0; i < 49; i++) {
        Frame_VSync();
    }
    CHECK(!RouteSelect_Resume(&selection, &f.assets, &raw));
    Frame_VSync();
    CHECK(RouteSelect_Resume(&selection, &f.assets, &raw));
    CHECK_EQ_INT(g_Session.route, 0);
    CHECK(g_Scene.active == &f.cache[1]);
    CHECK(g_Scene.active->layouts.stages[0].cells[0][1].card);
    CHECK(g_Stage.cleared && g_Player.is_hit);
    CHECK(RouteSelect_Resume(&selection, &f.assets, &raw));
    Arena_Free(&arena);
}

TEST(dual_direction_toggles_once_and_holds_do_not_repeat)
{
    ARENA arena;
    Arena_Init(&arena, "route", 2 * 1024 * 1024);
    FIXTURE f;
    M_Init(&f, &arena);
    Frame_Reset();
    g_VSync.count1 = 50;
    ROUTE_SELECT selection = {};
    const TH01_INPUT both = { .up = true, .down = true };
    CHECK(!RouteSelect_Begin(&selection, &f.assets, &both));
    CHECK_EQ_INT(selection.selection, 1);
    CHECK_EQ_INT(g_Video.pages[0][250 * VIDEO_WIDTH + 128], 4);
    CHECK_EQ_INT(g_Video.pages[0][300 * VIDEO_WIDTH + 128], 7);
    CHECK_EQ_INT(g_Palette.saved.colors[1][0], 9);
    CHECK_EQ_INT(g_Palette.saved.colors[3][0], 15);
    CHECK(!M_Poll(&selection, &f, both));
    CHECK_EQ_INT(selection.selection, 1);
    CHECK(!M_Poll(&selection, &f, (TH01_INPUT) { .down = true }));
    CHECK_EQ_INT(selection.selection, 1);
    CHECK(!M_Poll(&selection, &f, both));
    CHECK_EQ_INT(selection.selection, 0);
    CHECK(M_Poll(
        &selection, &f, (TH01_INPUT) { .up = true, .down = true, .ok = true }));
    CHECK_EQ_INT(g_Session.route, 0);
    Arena_Free(&arena);
}

TEST(transparent_scaled_spaces_preserve_backdrop)
{
    ARENA arena;
    Arena_Init(&arena, "route", 2 * 1024 * 1024);
    FIXTURE f;
    M_Init(&f, &arena);
    Frame_Reset();
    ROUTE_SELECT selection = {};
    const TH01_INPUT raw = {};
    CHECK(!RouteSelect_Begin(&selection, &f.assets, &raw));
    const GAME_TEXT_GLYPH *const r = GameText_Find(&f.font, 0xff32);
    for (int32_t y = 0; y < 32; y++) {
        for (int32_t x = 0; x < 32; x++) {
            CHECK_EQ_INT(
                g_Video.pages[0][(64 + y) * VIDEO_WIDTH + 64 + x],
                (r->rows[y / 2] & (0x8000u >> (x / 2))) != 0 ? 7 : 4);
            CHECK_EQ_INT(g_Video.pages[0][(64 + y) * VIDEO_WIDTH + 224 + x], 4);
        }
    }
    CHECK_EQ_INT(g_Video.drawing_page, 0);
    CHECK(!g_Stage.cleared && !g_Player.is_hit);
    Arena_Free(&arena);
}

TEST(input_side_effects_and_asset_validation_remain_real)
{
    ARENA arena;
    Arena_Init(&arena, "route", 2 * 1024 * 1024);
    FIXTURE f;
    M_Init(&f, &arena);
    ROUTE_SELECT_ASSETS bad = f.assets;
    bad.makai = &f.cache[2];
    CHECK(!Result_Ignore(RouteSelect_ValidateAssets(&bad)));
    bad = f.assets;
    const GAME_TEXT_FONT missing = {};
    bad.font = &missing;
    CHECK(!Result_Ignore(RouteSelect_ValidateAssets(&bad)));
    Frame_Reset();
    g_VSync.count1 = 50;
    ROUTE_SELECT selection = {};
    const TH01_INPUT pause = { .cancel = true };
    CHECK(!RouteSelect_Begin(&selection, &f.assets, &pause));
    CHECK(g_GameInput.paused);
    CHECK(M_Poll(&selection, &f, (TH01_INPUT) { .shot = true, .ok = true }));
    CHECK(g_Stage.cleared && g_Player.is_hit);
    Arena_Free(&arena);
}
