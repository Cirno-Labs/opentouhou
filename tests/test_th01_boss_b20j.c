#include "th01/boss_b20j.h"
#include "th01/shots.h"
#include "th01/player.h"
#include "th01/pellets.h"
#include "th01/orb.h"
#include "th01/rng.h"
#include "th01/session.h"

#include "core/base/arena.h"

#include "th01_world.h"
#include "test.h"

#include <string.h>

static void M_Assets(B20J_ASSETS *const a, ARENA *const arena)
{
    *a = (B20J_ASSETS) {};
    a->head = (BOS) { 80, 80, 3, Arena_AllocZ(arena, 80 * 80 * 3),
                      Arena_AllocZ(arena, 80 * 80 * 3) };
    a->face = (BOS) { 80, 32, 6, Arena_AllocZ(arena, 80 * 32 * 6),
                      Arena_AllocZ(arena, 80 * 32 * 6) };
    a->aim = (BOS) { 80, 32, 3, Arena_AllocZ(arena, 80 * 32 * 3),
                     Arena_AllocZ(arena, 80 * 32 * 3) };
    a->body.count = 16;
    uint8_t *const pixels = Arena_AllocZ(arena, VIDEO_PAGE_SIZE);
    uint8_t *const coverage = Arena_AllocZ(arena, VIDEO_PAGE_SIZE);
    for (int32_t i = 0; i < 16; i++) {
        a->body.images[i] =
            (GRX) { .stream_count = 1, .pixels = pixels, .coverage = coverage };
    }
    PI_IMAGE *const images[] = { &a->scroll,    &a->upper,     &a->layers[0],
                                 &a->layers[1], &a->layers[2], &a->layers[3] };
    for (int32_t i = 0; i < 6; i++) {
        *images[i] = (PI_IMAGE) {
            .width = 640, .height = 400, .has_palette = true, .pixels = pixels
        };
    }
    a->diamond = pixels;
    a->flake = pixels;
}

TEST(setup_resets_hp_and_face_but_preserves_history)
{
    ARENA arena;
    Arena_Init(&arena, "Konngara", 1024 * 1024);
    B20J_ASSETS assets;
    M_Assets(&assets, &arena);
    PALETTE_STATE palette = {};
    palette.saved.colors[10][1] = 7;
    memset(&g_HudHp, 0, sizeof(g_HudHp));
    const BOSS_HIT_CONTEXT hit = { .palette = &palette };
    const B20J_CONTEXT c = { .assets = &assets, .hit = &hit };
    B20J_Init();
    g_B20J.hit = (BOSS_HIT) { 27, true };
    g_B20J.pattern_state = 32;
    g_B20J.diamond_edges_frame = 91;
    g_B20J.two_snakes.left[1][3] = 172;
    g_B20J.ray_previous_x = 44;
    g_B20J.finished = true;
    const size_t used = arena.used;
    B20J_Setup(&c);
    CHECK_EQ_INT(arena.used, used);
    CHECK_EQ_INT(g_B20J.hp, 18);
    CHECK_EQ_INT(g_B20J.phase, B20J_PHASE_START);
    CHECK_EQ_INT(g_B20J.face_direction, 2);
    CHECK_EQ_INT(g_B20J.face_expression, 0);
    CHECK(g_B20J.direction_can_change);
    CHECK(!g_B20J.finished);
    CHECK_EQ_INT(g_HudHp.first_white, 16);
    CHECK_EQ_INT(g_HudHp.first_redwhite, 10);
    CHECK_EQ_INT(g_B20J.boss_palette.colors[10][1], 7);
    CHECK_EQ_INT(g_B20J.hit.frame, 27);
    CHECK(g_B20J.hit.invincible);
    CHECK_EQ_INT(g_B20J.pattern_state, 32);
    CHECK_EQ_INT(g_B20J.diamond_edges_frame, 91);
    CHECK_EQ_INT(g_B20J.two_snakes.left[1][3], 172);
    CHECK_EQ_INT(g_B20J.ray_previous_x, 44);
    Arena_Free(&arena);
}

TEST(validation_requires_all_sixteen_images_and_masks)
{
    ARENA arena;
    Arena_Init(&arena, "Konngara validation", 1024 * 1024);
    B20J_ASSETS a;
    M_Assets(&a, &arena);
    CHECK(Result_Ignore(B20J_ValidateAssets(&a)));
    a.body.count = 7;
    CHECK(!Result_Ignore(B20J_ValidateAssets(&a)));
    a.body.count = 16;
    a.body.images[15].coverage = nullptr;
    CHECK(!Result_Ignore(B20J_ValidateAssets(&a)));
    a.body.images[15].coverage = a.body.images[0].coverage;
    a.flake = nullptr;
    CHECK(!Result_Ignore(B20J_ValidateAssets(&a)));
    Arena_Free(&arena);
}

typedef struct {
    B20J_ASSETS assets;
    PALETTE_STATE palette;
    HUD_CONTEXT display;
    BOSS_HIT_CONTEXT hit;
    B20J_CONTEXT context;
    GAME_TEXT_FONT font;
    PTN stage;
    uint32_t high_score;
    int16_t lives;
    bool damage, cleared;
    int32_t effects[17], audio_count;
    B20J_AUDIO_ACTION audio[8];
} FIXTURE;

static void M_Effect(void *const user, const int16_t effect)
{
    FIXTURE *const f = user;
    CHECK(effect > 0 && effect <= 16);
    f->effects[effect]++;
}
static void M_Audio(void *const user, const B20J_AUDIO_ACTION action)
{
    FIXTURE *const f = user;
    CHECK(f->audio_count < 8);
    f->audio[f->audio_count++] = action;
}
static void M_Shot(void *const user, const SHOTS_ACTION action)
{
    FIXTURE *const f = user;
    if (action.kind == SHOTS_ACTION_EFFECT) {
        M_Effect(f, action.effect);
    } else {
        Shots_RenderAction(&f->stage, action);
    }
}
static void M_Fixture(FIXTURE *const f, ARENA *const arena)
{
    *f = (FIXTURE) {};
    Th01Test_ResetWorld();
    M_Assets(&f->assets, arena);
    const ALLOCATOR *const a = Arena_Allocator(arena);
    Video_Init(a);
    Tram_Init(a);
    Palette_Init(&f->palette);
    Hud_Init(a);
    HudHp_Init(a);
    GAME_TEXT_GLYPH glyphs[10] = {};
    for (uint32_t i = 0; i < 10; i++) {
        glyphs[i].codepoint = 0xff10 + i;
        glyphs[i].width = 16;
        for (int32_t y = 0; y < 16; y++) {
            glyphs[i].rows[y] = 0x8000u >> ((i + y) % 16);
        }
    }
    CHECK(Result_Ignore(GameText_LoadGlyphs(glyphs, 10, a, &f->font)));
    f->stage = (PTN) { .count = 4, .pixels = Allocator_Alloc(a, 4096) };
    memset(f->stage.pixels, PTN_TRANSPARENT, 4096);
    g_Player.left = 300;
    g_Player.invincible = true;
    f->lives = 3;
    g_Orb.top = 64;
    g_Orb.previous_top = 64;
    f->display = (HUD_CONTEXT) { .sprites = &f->stage,
                                 .font = &f->font,
                                 .high_score = &f->high_score,
                                 .remaining_lives = &f->lives };
    f->hit = (BOSS_HIT_CONTEXT) { .palette = &f->palette,
                                  .display = &f->display,
                                  .shots_output = { M_Shot, f },
                                  .test_damage = &f->damage,
                                  .frame_rand = 0x12345678,
                                  .effect = M_Effect,
                                  .user = f };
    f->context = (B20J_CONTEXT) { .assets = &f->assets,
                                  .hit = &f->hit,
                                  .game_cleared = &f->cleared,
                                  .audio = M_Audio };
    B20J_Init();
    B20J_Setup(&f->context);
    g_Session.settings.difficulty = f->display.rank;
}

TEST(entrance_retains_stage_palette_and_suspends_without_repeating_effects)
{
    ARENA arena;
    Arena_Init(&arena, "Konngara entrance", 2 * 1024 * 1024);
    FIXTURE f;
    M_Fixture(&f, &arena);
    f.assets.scroll.palette[10] = (GFX_COLOR) { 0, 119, 0, 255 };
    f.assets.upper.palette[10] = (GFX_COLOR) { 221, 0, 0, 255 };
    f.assets.layers[3].palette[10] = (GFX_COLOR) { 34, 51, 68, 255 };
    g_StageObjects.portal_active = true;
    g_StageObjects.destination_left = 77;
    g_Rng = 91;
    Video_SelectDrawingPage(1);
    const size_t used = arena.used;
    B20J_BeginEntrance(&f.context);
    CHECK_EQ_INT(g_B20J.head.sprites.width_bytes, 10);
    CHECK_EQ_INT(g_B20J.stage_palette.colors[10][1], 7);
    CHECK_EQ_INT(g_Rng, 91);
    CHECK(g_StageObjects.entrance_done && g_StageObjects.portal_active);
    CHECK_EQ_INT(g_StageObjects.destination_left, 77);
    CHECK_EQ_INT(f.audio_count, 2);
    CHECK_EQ_INT(f.audio[0], B20J_AUDIO_LOAD_MUSIC);
    CHECK_EQ_INT(f.audio[1], B20J_AUDIO_PLAY);
    Frame_Reset();
    int32_t polls = 0;
    while (!B20J_ResumeEntrance(&f.context)) {
        const B20J snapshot = g_B20J;
        const int32_t effects = f.effects[9];
        for (int32_t i = 0; i < 3; i++) {
            CHECK(!B20J_ResumeEntrance(&f.context));
            CHECK(memcmp(&g_B20J, &snapshot, sizeof(g_B20J)) == 0);
            CHECK_EQ_INT(f.effects[9], effects);
            CHECK_EQ_INT(f.audio_count, 2);
        }
        Frame_VSync();
        CHECK(++polls < 2000);
    }
    CHECK_EQ_INT(f.effects[9], 4);
    CHECK_EQ_INT(f.palette.resource.colors[10][0], 2);
    CHECK_EQ_INT(f.palette.saved.colors[10][0], 13);
    CHECK_EQ_INT(g_B20J.stage_palette.colors[10][1], 7);
    CHECK_EQ_INT(arena.used, used);
    CHECK(memcmp(g_Video.pages[0], g_Video.pages[1], VIDEO_PAGE_SIZE) == 0);
    CHECK_EQ_INT(g_B20J.hp, 18);
    Arena_Free(&arena);
}

TEST(all_patterns_complete_with_real_projectile_pools_on_every_rank)
{
    ARENA arena;
    Arena_Init(&arena, "Konngara patterns", 2 * 1024 * 1024);
    for (int32_t rank = 0; rank < 4; rank++) {
        for (int16_t pattern = 0; pattern < 12; pattern++) {
            Arena_Reset(&arena);
            FIXTURE f;
            M_Fixture(&f, &arena);
            f.display.rank = rank;
            g_Session.settings.difficulty = rank;
            g_B20J.phase = B20J_PHASE_FINAL;
            g_B20J.pattern = pattern;
            g_B20J.frame = 0;
            g_B20J.hit.frame = 0;
            g_B20J.patterns_done = 1;
            const size_t used = arena.used;
            Frame_Reset();
            int32_t frames = 0;
            do {
                CHECK(B20J_UpdateAndRender(&f.context));
                for (int32_t i = 0; i < PELLETS_COUNT; i++) {
                    g_Pellets.slots[i].moving = false;
                }
                for (int32_t i = 0; i < 10; i++) {
                    g_ShootoutLasers.slots[i].alive = false;
                }
                Frame_VSync();
                CHECK(++frames < 1500);
            } while (g_B20J.pattern != 99);
            CHECK_EQ_INT(g_B20J.frame, 0);
            CHECK_EQ_INT(g_B20J.hp, 18);
            CHECK_EQ_INT(arena.used, used);
            CHECK(!f.cleared);
        }
    }
    Arena_Free(&arena);
}

TEST(custom_final_defeat_finishes_bonus_and_stays_inert)
{
    ARENA arena;
    Arena_Init(&arena, "Konngara defeat", 2 * 1024 * 1024);
    FIXTURE f;
    M_Fixture(&f, &arena);
    g_B20J.phase = B20J_PHASE_FINAL;
    g_B20J.pattern = 99;
    g_B20J.frame = 0;
    g_B20J.hp = 0;
    g_Stage.score = UINT32_MAX - 100;
    f.assets.scroll.palette[7] = (GFX_COLOR) { 255, 255, 255, 255 };
    f.palette.saved.colors[7][0] = 15;
    Frame_Reset();
    const size_t used = arena.used;
    int32_t polls = 0;
    while (!B20J_UpdateAndRender(&f.context)) {
        const B20J snapshot = g_B20J;
        const int32_t effects = f.effects[7], audio_count = f.audio_count;
        CHECK(!f.cleared);
        for (int32_t i = 0; i < 3; i++) {
            CHECK(!B20J_UpdateAndRender(&f.context));
            CHECK(memcmp(&g_B20J, &snapshot, sizeof(g_B20J)) == 0);
            CHECK_EQ_INT(f.effects[7], effects);
            CHECK_EQ_INT(f.audio_count, audio_count);
        }
        Frame_VSync();
        CHECK(++polls < 1200);
    }
    CHECK(f.cleared && g_B20J.finished && !g_B20J.defeating);
    CHECK_EQ_INT(g_Stage.score, 49899);
    CHECK_EQ_INT(g_Video.scroll_top, 0);
    CHECK_EQ_INT(f.audio_count, 2);
    CHECK_EQ_INT(f.audio[0], B20J_AUDIO_STOP);
    CHECK_EQ_INT(f.audio[1], B20J_AUDIO_FADE);
    CHECK_EQ_INT(g_B20J.frame, 65);
    CHECK_EQ_INT(g_B20J.scroll_frame, 264);
    const B20J snapshot = g_B20J;
    for (int32_t i = 0; i < 100; i++) {
        Frame_VSync();
        CHECK(B20J_UpdateAndRender(nullptr));
        CHECK(memcmp(&g_B20J, &snapshot, sizeof(g_B20J)) == 0);
    }
    CHECK_EQ_INT(arena.used, used);
    Arena_Free(&arena);
}

TEST(entrance_pan_copies_visible_alias_after_hidden_rows)
{
    ARENA arena;
    Arena_Init(&arena, "Konngara pan", 2 * 1024 * 1024);
    FIXTURE f;
    M_Fixture(&f, &arena);
    memset(g_Video.pages[1], 6, VIDEO_PAGE_SIZE);
    g_B20J.entrance_active = true;
    g_B20J.entrance_stage = B20J_ENTRANCE_PAN_STEP;
    g_B20J.scroll_line = 400;
    Frame_Reset();
    CHECK(!B20J_ResumeEntrance(&f.context));
    CHECK_EQ_INT(g_Video.pages[0][0], 6);
    CHECK_EQ_INT(g_Video.pages[0][14335], 6);
    CHECK_EQ_INT(g_Video.pages[0][14336], 0);
    CHECK_EQ_INT(g_Video.drawing_page, 0);
    CHECK_EQ_INT(g_B20J.scroll_line, 368);
    Arena_Free(&arena);
}

TEST(hit_tests_current_orb_at_inclusive_rectangle_edges)
{
    ARENA arena;
    Arena_Init(&arena, "Konngara hit", 2 * 1024 * 1024);
    const int16_t points[][2] = { { 288, 120 }, { 352, 160 }, { 287, 120 },
                                  { 353, 160 }, { 288, 119 }, { 352, 161 } };
    for (int32_t i = 0; i < 6; i++) {
        Arena_Reset(&arena);
        FIXTURE f;
        M_Fixture(&f, &arena);
        g_B20J.phase = B20J_PHASE_FINAL;
        g_B20J.pattern = 99;
        g_Orb.left = points[i][0];
        g_Orb.top = points[i][1];
        g_Orb.previous_left = 300;
        g_Orb.previous_top = 140;
        Frame_Reset();
        CHECK(B20J_UpdateAndRender(&f.context));
        CHECK_EQ_INT(g_B20J.hp, i < 2 ? 17 : 18);
        CHECK_EQ_INT(g_Stage.score, i < 2 ? 10000 : 0);
        CHECK_EQ_INT(f.effects[3], i < 2 ? 1 : 0);
    }
    Arena_Free(&arena);
}

TEST(last_orb_hit_draws_every_cached_kuji_then_adds_final_bonus)
{
    ARENA arena;
    Arena_Init(&arena, "Konngara final hit", 8 * 1024 * 1024);
    FIXTURE f;
    M_Fixture(&f, &arena);
    for (int32_t i = 7; i <= 15; i++) {
        GRX *const image = &f.assets.body.images[i];
        image->pixels = Arena_AllocZ(&arena, VIDEO_PAGE_SIZE);
        image->coverage = Arena_AllocZ(&arena, VIDEO_PAGE_SIZE);
        image->coverage[0] = 1;
        image->pixels[0] = i - 7;
    }
    g_B20J.phase = B20J_PHASE_FINAL;
    g_B20J.pattern = 99;
    g_B20J.hp = 1;
    g_Orb.left = 320;
    g_Orb.top = 140;
    g_Video.pages[0][0] = 9;
    Frame_Reset();
    const size_t used = arena.used;
    int32_t images = 0, last_image = -1;
    int32_t polls = 0;
    while (!B20J_UpdateAndRender(&f.context)) {
        if (g_B20J.defeat_stage == B20J_DEFEAT_KUJI
            && g_B20J.animation_frame != last_image) {
            CHECK_EQ_INT(g_Video.pages[0][0], images);
            last_image = g_B20J.animation_frame;
            images++;
        }
        Frame_VSync();
        CHECK(++polls < 1200);
    }
    CHECK_EQ_INT(images, 9);
    CHECK_EQ_INT(g_B20J.hp, 0);
    CHECK_EQ_INT(g_Stage.score, 60000);
    CHECK_EQ_INT(f.effects[3], 1);
    CHECK(f.cleared && g_B20J.finished);
    CHECK_EQ_INT(arena.used, used);
    Arena_Free(&arena);
}

TEST(siddham_layers_use_key_fifteen_and_retain_the_resource_palette)
{
    ARENA arena;
    Arena_Init(&arena, "Konngara overlays", 2 * 1024 * 1024);
    FIXTURE f;
    M_Fixture(&f, &arena);
    uint8_t pixels[] = { 15, 9 };
    for (int32_t i = 0; i < 4; i++) {
        f.assets.layers[i].pixels = pixels;
        f.assets.layers[i].width = 2;
        f.assets.layers[i].height = 1;
        f.assets.layers[i].transparent = 9;
        f.assets.layers[i].has_position = true;
        f.assets.layers[i].x = 80;
        f.assets.layers[i].y = 80;
    }
    f.assets.layers[3].palette[10] = (GFX_COLOR) { 51, 68, 85, 255 };
    g_Video.pages[0][0] = 6;
    g_B20J.entrance_active = true;
    g_B20J.entrance_stage = B20J_ENTRANCE_FLASH;
    Frame_Reset();
    CHECK(!B20J_ResumeEntrance(&f.context));
    CHECK_EQ_INT(g_Video.pages[0][0], 6);
    CHECK_EQ_INT(g_Video.pages[0][1], 9);
    CHECK_EQ_INT(f.palette.resource.colors[10][0], 3);
    CHECK_EQ_INT(f.palette.saved.colors[9][0], 14);
    CHECK_EQ_INT(f.palette.saved.colors[9][1], 14);
    CHECK_EQ_INT(f.palette.saved.colors[9][2], 14);
    Arena_Free(&arena);
}
