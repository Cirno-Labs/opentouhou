#include "../src/th01/boss_b20m.c"
#include "../src/th01/hud_hp.c"
#include "th01/gameplay.h"
#include "th01/sound.h"
#include "th01/shots.h"
#include "th01/player.h"
#include "th01/pellets.h"
#include "th01/orb.h"
#include "th01/rng.h"
#include "th01/session.h"
#include "core/base/arena.h"
#include "th01_world.h"
#include "test.h"
static void M_BossSetup(const B20M_ASSETS *const assets)
{
    const BOSS_CONTEXT ctx = {};
    g_BossSariel.setup(assets, &ctx);
}

static void M_BossBeginEntrance(const B20M_ASSETS *const assets)
{
    g_BossSariel.begin_entrance(assets);
}

static bool M_BossResumeEntrance(
    const B20M_ASSETS *const assets, const uint64_t host_tick)
{
    const BOSS_CONTEXT ctx = { .host_tick = host_tick };
    return g_BossSariel.resume_entrance(assets, &ctx);
}

static bool M_BossUpdate(
    const B20M_ASSETS *const assets, const uint64_t host_tick)
{
    const BOSS_CONTEXT ctx = { .host_tick = host_tick };
    return g_BossSariel.update(assets, &ctx);
}

static void M_Assets(B20M_ASSETS *const a, ARENA *const arena)
{
    *a = (B20M_ASSETS) {};
    uint8_t *const pixels = Arena_AllocZ(arena, VIDEO_PAGE_SIZE);
    uint8_t *const coverage = Arena_AllocZ(arena, VIDEO_PAGE_SIZE);
    a->shield = (BOS) { 48, 48, 4, pixels, coverage };
    a->wand = (BOS) { 96, 96, 2, pixels, coverage };
    a->dress = (BOS) { 96, 96, 4, pixels, coverage };
    a->vortex = (GRC) { 32, 32, 6, coverage };
    a->bird = (GRC) { 32, 32, 8, coverage };
    a->cross = (GRC) { 32, 32, 2, coverage };
    a->splash = (GRC) { 32, 32, 3, coverage };
    PI_IMAGE *const images[] = { &a->lower,          &a->upper,
                                 &a->backgrounds[0], &a->backgrounds[1],
                                 &a->backgrounds[2], &a->backgrounds[3],
                                 &a->backgrounds[4], &a->backgrounds[5] };
    for (int32_t i = 0; i < 8; i++) {
        *images[i] = (PI_IMAGE) {
            .width = 640, .height = 400, .has_palette = true, .pixels = pixels
        };
    }
    a->star = pixels;
    a->flake = pixels;
    a->spark = pixels;
    a->leaf_left = pixels;
    a->leaf_right = pixels;
    a->interleave = Arena_AllocZ(arena, sizeof(INTERLEAVE_ASSETS));
}
TEST(setup_preserves_patterns)
{
    ARENA arena;
    Arena_Init(&arena, "Sariel", 1024 * 1024);
    B20M_ASSETS a;
    M_Assets(&a, &arena);
    memset(&m_B20M, 0xa5, sizeof(m_B20M));
    g_BossSariel.init();
    CHECK_EQ_INT(m_B20M.ray_previous_x, 999);
    CHECK_EQ_INT(m_B20M.ray_previous_y, 999);
    CHECK_EQ_INT(m_B20M.flake.state, -1);
    m_B20M.pattern_state = 12;
    m_B20M.leaves.left[4] = 123;
    m_B20M.birds.left[3] = 25.5;
    m_B20M.shield.lock_frame = 7;
    m_B20M.hit.frame = 17;
    m_B20M.hit.invincible = true;
    memset(&g_Palette, 0, sizeof(g_Palette));
    memset(&m_Health, 0, sizeof(m_Health));
    g_Palette.saved.colors[6][0] = 9;

    const size_t used = arena.used;
    M_BossSetup(&a);
    CHECK(g_Boss == &m_B20M.status);
    CHECK_EQ_INT(arena.used, used);
    CHECK_EQ_INT(m_B20M.status.hp, 18);
    CHECK_EQ_INT(m_B20M.shield.left, 304);
    CHECK_EQ_INT(m_B20M.shield.top, 144);
    CHECK_EQ_INT(m_B20M.shield.orb_hitbox.left, 0);
    CHECK_EQ_INT(m_B20M.shield.orb_hitbox.right, 48);
    CHECK_EQ_INT(m_B20M.shield.lock_frame, 7);
    CHECK_EQ_INT(m_B20M.hit.frame, 17);
    CHECK(m_B20M.hit.invincible);
    CHECK_EQ_INT(m_B20M.pattern_state, 12);
    CHECK_EQ_INT(m_B20M.leaves.left[4], 123);
    CHECK(m_B20M.birds.left[3] == 25.5);
    CHECK_EQ_INT(m_B20M.wand.sprites.slot, 0);
    CHECK_EQ_INT(m_B20M.dress.sprites.slot, 1);
    CHECK_EQ_INT(m_B20M.wand.left, 296);
    CHECK_EQ_INT(m_B20M.dress.top, 192);
    CHECK_EQ_INT(m_Health.first_white, 8);
    CHECK_EQ_INT(m_Health.first_redwhite, 2);
    CHECK_EQ_INT(m_B20M.boss_palette.colors[6][0], 9);
    Arena_Free(&arena);
}
TEST(validation_requires_both_forms_and_all_cached_resources)
{
    ARENA arena;
    Arena_Init(&arena, "Sariel validation", 1024 * 1024);
    B20M_ASSETS a;
    M_Assets(&a, &arena);
    CHECK(Result_Ignore(M_ValidateAssets(&a)));
    a.backgrounds[5].pixels = nullptr;
    CHECK(!Result_Ignore(M_ValidateAssets(&a)));
    a.backgrounds[5].pixels = a.lower.pixels;
    a.spark = nullptr;
    CHECK(!Result_Ignore(M_ValidateAssets(&a)));
    a.spark = a.star;
    a.bird.count = 7;
    CHECK(!Result_Ignore(M_ValidateAssets(&a)));
    Arena_Free(&arena);
}

#include <string.h>
typedef struct {
    B20M_ASSETS assets;
    GAME_TEXT_FONT font;
    PTN stage;
    GRF mask;
} FIXTURE;

static void M_Fixture(FIXTURE *const f, ARENA *const arena)
{
    *f = (FIXTURE) {};
    Th01Test_ResetWorld();
    GameInput_Init();
    StageTimer_Init();
    M_Assets(&f->assets, arena);
    const ALLOCATOR *const a = Arena_Allocator(arena);
    Video_Init(a);
    Tram_Init(a);
    Hud_Init(a);
    HudHp_Init(a);
    GAME_TEXT_GLYPH glyphs[105] = {};
    for (uint32_t i = 0; i < 10; i++) {
        glyphs[i].codepoint = 0xff10 + i;
        glyphs[i].width = 16;
        for (int32_t y = 0; y < 16; y++) {
            glyphs[i].rows[y] = 0x8000u >> ((i + y) % 16);
        }
    }
    for (uint32_t i = 0; i < 95; i++) {
        glyphs[i + 10].codepoint = 32 + i;
        glyphs[i + 10].width = 8;
        for (int32_t y = 0; y < 16; y++) {
            glyphs[i + 10].rows[y] = 0xaa00;
        }
    }
    CHECK(Result_Ignore(GameText_LoadGlyphs(glyphs, 105, a, &f->font)));
    f->stage = (PTN) { .count = 4, .pixels = Allocator_Alloc(a, 4096) };
    memset(f->stage.pixels, PTN_TRANSPARENT, 4096);
    f->mask.coverage = Allocator_AllocZ(a, 640 * 66);
    g_Player.left = 300;
    g_Player.invincible = true;
    g_Session.lives = 3;
    g_Orb.top = 64;
    g_Orb.previous_top = 64;
    Hud_Load(&f->mask, &f->stage, &f->font);
    g_Session.stage_id = 19;
    g_Session.frame_rand = 0x12345678;
    g_BossSariel.init();
    M_BossSetup(&f->assets);
    g_Session.settings.difficulty = SETTINGS_DIFFICULTY_EASY;
}

static bool M_Tick(FIXTURE *const f, uint64_t *const tick)
{
    const bool done = M_BossUpdate(&f->assets, *tick);
    (*tick)++;
    Frame_VSync();
    return done;
}
TEST(cached_scroll_entrance_and_diagonal_wait_without_allocating)
{
    ARENA arena;
    Arena_Init(&arena, "Sariel entrance", 4 * 1024 * 1024);
    FIXTURE f;
    M_Fixture(&f, &arena);
    memset(f.assets.lower.pixels, 12, VIDEO_PAGE_SIZE);
    g_Rng = 5;
    Frame_Reset();
    uint64_t tick = 0;
    memset(&g_StagePalette, 0x07, sizeof(g_StagePalette));
    const PALETTE before = g_StagePalette;
    const size_t used = arena.used;
    M_BossBeginEntrance(&f.assets);
    CHECK(g_Pellets.interlace);
    CHECK(!M_BossResumeEntrance(&f.assets, tick));
    const B20M pending = m_B20M;
    CHECK(!M_BossResumeEntrance(&f.assets, tick));
    CHECK(memcmp(&m_B20M, &pending, sizeof(m_B20M)) == 0);
    int32_t polls = 0;
    while (m_B20M.entrance_active && polls++ < 1500) {
        CHECK(memcmp(&g_StagePalette, &before, sizeof(before)) == 0);
        tick++;
        Frame_VSync();
        M_BossResumeEntrance(&f.assets, tick);
    }
    CHECK(!m_B20M.entrance_active);
    CHECK_EQ_INT(g_Rng, g_Session.frame_rand);
    CHECK_EQ_INT(g_Video.drawing_page, 0);
    CHECK_EQ_INT(g_Video.scroll_top, 0);
    CHECK_EQ_INT(arena.used, used);
    CHECK(memcmp(&g_StagePalette, &m_B20M.stage_palette, sizeof(PALETTE)) == 0);
    Arena_Free(&arena);
}
TEST(ring_entrance_preserves_pending_polls_and_reserves_wand_background)
{
    ARENA arena;
    Arena_Init(&arena, "Sariel rings", 4 * 1024 * 1024);
    FIXTURE f;
    M_Fixture(&f, &arena);
    Frame_Reset();
    uint64_t tick = 0;
    const size_t used = arena.used;
    CHECK(!M_BossUpdate(&f.assets, tick));
    const B20M pending = m_B20M;
    const uint32_t rng = g_Rng;
    CHECK(!M_BossUpdate(&f.assets, tick));
    CHECK(memcmp(&m_B20M, &pending, sizeof(m_B20M)) == 0);
    CHECK_EQ_INT(g_Rng, rng);
    for (int32_t i = 0; m_B20M.sequence != B20M_SEQUENCE_NONE && i < 200; i++) {
        tick++;
        Frame_VSync();
        M_BossUpdate(&f.assets, tick);
    }
    CHECK_EQ_INT(m_B20M.status.phase, B20M_PHASE_BIRDS);
    CHECK_EQ_INT(m_B20M.status.frame, 0);
    CHECK_EQ_INT(m_B20M.entrance_radius, 352);
    CHECK_EQ_INT(m_B20M.shield.left, 304);
    CHECK(m_B20M.patterns_until_next >= 1 && m_B20M.patterns_until_next <= 6);
    CHECK_EQ_INT(arena.used, used);
    Arena_Free(&arena);
}
TEST(all_first_form_patterns_finish_with_real_projectile_pools)
{
    const int8_t phases[] = {
        B20M_PHASE_BIRDS,
        B20M_PHASE_VERTICAL,
        B20M_PHASE_WAVY,
        B20M_PHASE_VERTICAL_BIRDS,
    };
    const int16_t counts[] = { 3, 2, 6, 3 };
    for (int32_t rank = 0; rank < 4; rank++) {
        for (int32_t phase = 0; phase < 4; phase++) {
            for (int16_t pattern = 0; pattern < counts[phase]; pattern++) {
                ARENA arena;
                Arena_Init(&arena, "Sariel first form", 4 * 1024 * 1024);
                FIXTURE f;
                M_Fixture(&f, &arena);
                g_Session.settings.difficulty = rank;
                g_Session.settings.difficulty = rank;
                g_Rng = 0x12345678u + (uint32_t)rank;
                m_B20M.status.phase = phases[phase];
                m_B20M.pattern = pattern;
                m_B20M.patterns_until_next = 30000;
                Frame_Reset();
                uint64_t tick = 0;
                const size_t used = arena.used;
                int32_t calls = 0;
                while (m_B20M.pattern == pattern && calls++ < 2000) {
                    M_Tick(&f, &tick);
                }
                CHECK(calls < 2000);
                CHECK_EQ_INT(m_B20M.status.hp, 18);
                CHECK_EQ_INT(m_B20M.status.phase, phases[phase]);
                CHECK_EQ_INT(arena.used, used);
                Arena_Free(&arena);
            }
        }
    }
}
TEST(all_second_form_patterns_finish_with_persistent_leaf_state)
{
    for (int32_t rank = 0; rank < 4; rank++) {
        for (int16_t pattern = 0; pattern < 9; pattern++) {
            ARENA arena;
            Arena_Init(&arena, "Sariel second form", 4 * 1024 * 1024);
            FIXTURE f;
            M_Fixture(&f, &arena);
            g_Session.settings.difficulty = rank;
            g_Session.settings.difficulty = rank;
            g_Rng = 0xabcdef01u + (uint32_t)rank;
            m_B20M.status.phase = B20M_PHASE_SECOND_FORM;
            m_B20M.status.hp = 6;
            m_B20M.pattern = pattern;
            m_B20M.pattern_state = 24;
            Frame_Reset();
            uint64_t tick = 0;
            const size_t used = arena.used;
            int32_t calls = 0;
            while (m_B20M.pattern == pattern && calls++ < 1000) {
                M_Tick(&f, &tick);
            }
            CHECK(calls < 1000);
            CHECK_EQ_INT(m_B20M.status.hp, 6);
            CHECK_EQ_INT(m_B20M.status.phase, B20M_PHASE_SECOND_FORM);
            CHECK_EQ_INT(arena.used, used);
            Arena_Free(&arena);
        }
    }
}
TEST(form_change_resets_player_and_flight_but_retains_previous_orb)
{
    ARENA arena;
    Arena_Init(&arena, "Sariel transform", 4 * 1024 * 1024);
    FIXTURE f;
    M_Fixture(&f, &arena);
    m_B20M.status.phase = B20M_PHASE_TRANSFORM;
    m_B20M.status.hp = 0;
    g_Orb.previous_left = 123;
    g_Orb.previous_top = 234;
    g_Player.invincible = true;
    g_Player.invincibility_time = 91;
    g_Player.deflecting = true;
    g_Player.bomb_damaging = true;
    m_B20M.leaves.flag[7] = 6;
    g_GameInput.bomb = true;
    Frame_Reset();
    uint64_t tick = 0;
    const size_t used = arena.used;
    CHECK(!M_BossUpdate(&f.assets, tick));
    CHECK_EQ_INT(g_Player.left, 304);
    CHECK_EQ_INT(g_Orb.left, 600);
    CHECK_EQ_INT(g_Orb.top, 280);
    CHECK_EQ_INT(g_Orb.previous_left, 123);
    CHECK_EQ_INT(g_Orb.previous_top, 234);
    CHECK(!g_Player.deflecting);
    CHECK(!g_Player.bomb_damaging);
    CHECK(!g_GameInput.bomb);
    for (int32_t i = 0; m_B20M.sequence != B20M_SEQUENCE_NONE && i < 1000;
         i++) {
        tick++;
        Frame_VSync();
        M_BossUpdate(&f.assets, tick);
    }
    CHECK_EQ_INT(m_B20M.status.phase, B20M_PHASE_SECOND_FORM);
    CHECK_EQ_INT(m_B20M.status.hp, 6);
    CHECK_EQ_INT(m_B20M.status.frame, 0);
    CHECK_EQ_INT(g_Player.invincibility_time, 0);
    CHECK(!g_Player.invincible);
    CHECK_EQ_INT(m_B20M.leaves.flag[7], 0);
    CHECK_EQ_INT(m_Health.first_white, 10);
    CHECK_EQ_INT(m_Health.first_redwhite, 3);
    CHECK(g_Stage.first_stage);
    CHECK_EQ_INT(arena.used, used);
    Arena_Free(&arena);
}
TEST(final_current_orb_hit_finishes_custom_victory_and_becomes_inert)
{
    ARENA arena;
    Arena_Init(&arena, "Sariel victory", 4 * 1024 * 1024);
    FIXTURE f;
    M_Fixture(&f, &arena);
    m_B20M.status.phase = B20M_PHASE_SECOND_FORM;
    m_B20M.status.hp = 1;
    g_Orb.left = 320;
    g_Orb.top = 120;
    Frame_Reset();
    uint64_t tick = 0;
    const size_t used = arena.used;
    CHECK(M_BossUpdate(&f.assets, tick));
    CHECK_EQ_INT(m_B20M.status.phase, B20M_PHASE_DEFEAT);
    CHECK_EQ_INT(m_B20M.status.hp, 0);
    CHECK_EQ_INT(g_Stage.score, 10000);
    for (int32_t i = 0; !g_Stage.game_cleared && i < 1500; i++) {
        M_Tick(&f, &tick);
    }
    CHECK(g_Stage.game_cleared);
    CHECK(m_B20M.finished);
    CHECK_EQ_INT(g_Stage.score, 60000);
    CHECK_EQ_INT(m_B20M.status.frame, 231);
    CHECK_EQ_INT(g_Video.scroll_top, 0);
    CHECK_EQ_INT(arena.used, used);
    const B20M done = m_B20M;
    CHECK(M_BossUpdate(&f.assets, tick));
    CHECK(memcmp(&m_B20M, &done, sizeof(m_B20M)) == 0);
    CHECK_EQ_INT(g_Stage.score, 60000);
    Arena_Free(&arena);
}
TEST(bird_collision_waits_before_pattern_rng_and_hit_tail)
{
    ARENA arena;
    Arena_Init(&arena, "Sariel bird safety", 4 * 1024 * 1024);
    FIXTURE f;
    M_Fixture(&f, &arena);
    m_B20M.status.phase = B20M_PHASE_BIRDS;
    m_B20M.status.frame = 49;
    m_B20M.pattern = 0;
    m_B20M.patterns_until_next = 10;
    g_Player.invincible = false;
    m_B20M.birds.alive[0] = true;
    m_B20M.birds.left[0] = 300;
    m_B20M.birds.top[0] = 368;
    Frame_Reset();
    uint64_t tick = 100;
    const uint32_t rng = g_Rng;
    const size_t used = arena.used;
    CHECK(!M_BossUpdate(&f.assets, tick));
    CHECK(g_Player.is_hit);
    CHECK_EQ_INT(g_Rng, rng);
    const B20M pending = m_B20M;
    CHECK(!M_BossUpdate(&f.assets, tick + 5));
    CHECK(memcmp(&m_B20M, &pending, sizeof(m_B20M)) == 0);
    CHECK(M_BossUpdate(&f.assets, tick + 6));
    CHECK(g_Rng != rng);
    CHECK_EQ_INT(m_B20M.status.frame, 50);
    CHECK_EQ_INT(m_B20M.resume_stage, B20M_RESUME_NONE);
    CHECK_EQ_INT(arena.used, used);
    Arena_Free(&arena);
}
