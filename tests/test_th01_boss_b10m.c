#include "../src/th01/boss_b10m.c"
#include "../src/th01/hud_hp.c"
#include "../src/th01/boss_entity.c"
#include "../src/th01/shootout_lasers.c"
#include "../src/th01/scene.c"
#include "../src/th01/missiles.c"
#include "th01/sound.h"
#include "th01/shots.h"
#include "th01/player.h"
#include "th01/pellets.h"
#include "th01/orb.h"
#include "th01/rng.h"
#include "th01/session.h"
#include "pc98/zun/master_rng.h"
#include "pc98/zun/math16.h"

#include "core/base/arena.h"

#include "th01_video.h"
#include "th01_world.h"
#include "test.h"

static void M_BossSetup(const B10M_ASSETS *const assets, const PTN *const miko)
{
    const BOSS_CONTEXT ctx = { .miko = miko };
    g_BossYuugenMagan.setup(assets, &ctx);
}

static bool M_BossUpdate(const B10M_ASSETS *const assets, const PTN *const miko)
{
    const BOSS_CONTEXT ctx = { .miko = miko };
    return g_BossYuugenMagan.update(assets, &ctx);
}

typedef struct {
    B10M_ASSETS assets;
    GAME_TEXT_FONT font;
    PTN stage, miko;
} FIXTURE;

static void M_InitFixture(FIXTURE *const f, ARENA *const arena)
{
    *f = (FIXTURE) {};
    Th01Test_ResetWorld();
    const ALLOCATOR *const a = Arena_Allocator(arena);
    Video_Init(a);
    f->assets.eyes = (BOS) { 64, 48, 7, Arena_AllocZ(arena, 64 * 48 * 7),
                             Arena_AllocZ(arena, 64 * 48 * 7) };
    f->assets.missiles =
        (PTN) { .count = 5, .pixels = Arena_AllocZ(arena, 5 * 1024) };
    f->assets.background =
        (PI_IMAGE) { .width = 640,
                     .height = 400,
                     .has_palette = true,
                     .pixels = Arena_AllocZ(arena, VIDEO_PAGE_SIZE) };
    uint8_t bytes[22 + 5 * 250] = {};
    READER reader;
    Reader_OpenBuffer(&reader, bytes, sizeof(bytes), nullptr);
    CHECK(
        Result_Ignore(Scene_LoadReader(&reader, 3, a, &f->assets.next_scene)));
    Reader_Close(&reader);
    for (int32_t color = 0; color < 16; color++) {
        for (int32_t component = 0; component < 3; component++) {
            f->assets.palette.colors[color][component] =
                (int8_t)((color + component * 3) % 16);
        }
    }
}

static void M_Battle(FIXTURE *const f, ARENA *const arena)
{
    M_InitFixture(f, arena);
    const ALLOCATOR *const a = Arena_Allocator(arena);
    Hud_Init(a);
    HudHp_Init(a);
    GAME_TEXT_GLYPH glyphs[10] = {};
    for (uint32_t i = 0; i < 10; i++) {
        glyphs[i].codepoint = 0xff10 + i;
        glyphs[i].width = 16;
        for (int32_t y = 0; y < 16; y++) {
            glyphs[i].rows[y] = (uint16_t)(0x8000u >> ((i + y) % 16));
        }
    }
    CHECK(Result_Ignore(GameText_LoadGlyphs(glyphs, 10, a, &f->font)));
    f->stage = (PTN) { .count = 4, .pixels = Allocator_Alloc(a, 4096) };
    memset(f->stage.pixels, PTN_TRANSPARENT, 4096);
    f->miko = (PTN) { .count = 1, .pixels = Allocator_Alloc(a, 1024) };
    memset(f->miko.pixels, PTN_TRANSPARENT, 1024);
    g_Session.lives = 3;
    g_Rng = 0xb10a0000;
    g_Player.invincible = true;
    g_Player.left = 300;
    g_Orb.top = 64;
    g_Orb.previous_top = 64;
    Hud_Load(nullptr, &f->stage, &f->font);
    g_Session.settings.difficulty = SETTINGS_DIFFICULTY_EASY;
}

static void M_Enter(FIXTURE *const f)
{
    for (int32_t frame = 0; frame < 331; frame++) {
        g_Session.frame_rand++;
        CHECK(M_BossUpdate(&f->assets, &f->miko));
    }
    CHECK_EQ_INT(m_B10M.status.phase, B10M_PHASE_LATERAL);
    CHECK_EQ_INT(m_B10M.status.frame, 0);
}

TEST(setup_shares_eye_assets_and_retains_static_history)
{
    ARENA arena;
    Arena_Init(&arena, "b10m", 2 * 1024 * 1024);
    FIXTURE f;
    M_InitFixture(&f, &arena);
    g_BossYuugenMagan.init();
    m_B10M.status.frame = 77;
    m_B10M.status.phase = B10M_PHASE_STAR_ATTACK;
    m_B10M.pattern_state = 17;
    m_B10M.subphase = 8;
    m_B10M.hit = (BOSS_HIT) { 39, true };
    m_B10M.hp_bar_filled = true;
    m_B10M.pentagram.angle = 450;
    m_B10M.pentagram.phase = B10M_STAR_GROWN_SLAM;
    for (int32_t i = 0; i < 5; i++) {
        m_B10M.eyes[i].lock_frame = (int16_t)(i % 4);
        m_B10M.eyes[i].previous_left = (int16_t)(i * 11);
        m_Missiles.slots[i].flag = MISSILE_MOVING;
        m_Missiles.slots[i].previous_top = (int16_t)(i * 9);
    }
    memset(g_Video.pages[0], 3, VIDEO_PAGE_SIZE);
    memset(g_Video.pages[1], 5, VIDEO_PAGE_SIZE);
    const size_t used = arena.used;
    M_BossSetup(&f.assets, &f.miko);
    CHECK(g_Boss == &m_B10M.status);
    CHECK_EQ_INT(arena.used, used);
    CHECK_EQ_INT(m_B10M.status.hp, 16);
    CHECK_EQ_INT(m_B10M.status.phase, B10M_PHASE_ENTRANCE);
    CHECK_EQ_INT(m_B10M.status.frame, 0);
    CHECK_EQ_INT(m_B10M.hit.frame, 39);
    CHECK(m_B10M.hit.invincible);
    CHECK(m_B10M.hp_bar_filled);
    CHECK_EQ_INT(m_B10M.pattern_state, 17);
    CHECK_EQ_INT(m_B10M.subphase, 8);
    CHECK_EQ_INT(m_B10M.pentagram.angle, 450);
    CHECK_EQ_INT(m_Health.first_white, 12);
    CHECK_EQ_INT(m_Health.first_redwhite, 8);
    CHECK(m_Sprites.entities[0] == &f.assets.eyes);
    const int16_t left[] = { 64, 512, 192, 384, 288 };
    const int16_t top[] = { 128, 128, 176, 176, 64 };
    for (int32_t i = 0; i < 5; i++) {
        CHECK_EQ_INT(m_B10M.eyes[i].left, left[i]);
        CHECK_EQ_INT(m_B10M.eyes[i].top, top[i]);
        CHECK_EQ_INT(m_B10M.eyes[i].sprites.slot, 0);
        CHECK_EQ_INT(m_B10M.eyes[i].sprites.width_bytes, 8);
        CHECK_EQ_INT(m_B10M.eyes[i].sprites.height, 48);
        CHECK_EQ_INT(m_B10M.eyes[i].image, 0);
        CHECK(m_B10M.eyes[i].orb_inactive);
        CHECK_EQ_INT(m_B10M.eyes[i].lock_frame, i % 4);
        CHECK_EQ_INT(m_B10M.eyes[i].previous_left, i * 11);
        CHECK_EQ_INT(m_Missiles.slots[i].flag, MISSILE_FREE);
        CHECK_EQ_INT(m_Missiles.slots[i].previous_top, i * 9);
    }
    CHECK_EQ_INT(g_Video.pages[0][0], 3);
    CHECK_EQ_INT(g_Video.pages[1][0], 5);
    Arena_Free(&arena);
}

TEST(asset_validation_rejects_missing_live_storage)
{
    ARENA arena;
    Arena_Init(&arena, "b10m", 2 * 1024 * 1024);
    FIXTURE f;
    M_InitFixture(&f, &arena);
    CHECK(Result_Ignore(M_ValidateAssets(&f.assets)));
    B10M_ASSETS a = f.assets;
    a.eyes.count = 6;
    CHECK(!Result_Ignore(M_ValidateAssets(&a)));
    a = f.assets;
    a.next_scene.id = 2;
    CHECK(!Result_Ignore(M_ValidateAssets(&a)));
    a = f.assets;
    a.missiles.count = 4;
    CHECK(!Result_Ignore(M_ValidateAssets(&a)));
    Arena_Free(&arena);
}

TEST(ordinary_entrance_preserves_world_and_rank_fire)
{
    for (int32_t rank = 0; rank < 4; rank++) {
        ARENA arena;
        Arena_Init(&arena, "b10m", 2 * 1024 * 1024);
        FIXTURE f;
        M_Battle(&f, &arena);
        g_Session.settings.difficulty = rank;
        g_Session.settings.difficulty = rank;
        g_BossYuugenMagan.init();
        M_BossSetup(&f.assets, &f.miko);
        const size_t used = arena.used;
        VSyncIrq_Reset();
        const ORB orb = g_Orb;
        M_Enter(&f);
        CHECK_EQ_INT(g_VSync->count1, 0);
        CHECK(memcmp(&orb, &g_Orb, sizeof(orb)) == 0);
        CHECK(!g_Player.is_hit);
        CHECK_EQ_INT(g_Stage.score, 0);
        CHECK_EQ_INT(arena.used, used);
        CHECK_EQ_INT(
            m_B10M.pattern_interval,
            rank == 0       ? 350
                : rank == 1 ? 300
                : rank == 2 ? 200
                            : 130);
        int32_t count = 0;
        for (int32_t i = 0; i < PELLETS_COUNT; i++) {
            count += g_Pellets.slots[i].moving;
        }
        CHECK_EQ_INT(count, rank == 2 ? 15 : rank == 3 ? 25 : 0);
        CHECK_EQ_INT(m_B10M.eyes[0].image, 4);
        CHECK_EQ_INT(m_B10M.eyes[1].image, 3);
        CHECK(!m_B10M.eyes[0].orb_inactive);
        CHECK(m_B10M.eyes[4].orb_inactive);
        Arena_Free(&arena);
    }
}

TEST(full_rank_fights_run_patterns_and_both_star_cycles)
{
    for (int32_t rank = 0; rank < 4; rank++) {
        ARENA arena;
        Arena_Init(&arena, "b10m", 2 * 1024 * 1024);
        FIXTURE f;
        M_Battle(&f, &arena);
        g_Session.settings.difficulty = rank;
        g_Session.settings.difficulty = rank;
        g_BossYuugenMagan.init();
        M_BossSetup(&f.assets, &f.miko);
        VSyncIrq_Reset();
        const size_t used = arena.used;
        uint16_t phases = 0, star_phases = 0;
        int32_t loops = 0;
        int16_t previous_star_phase = -1;
        for (int32_t frame = 0; frame < 18000 && loops < 2; frame++) {
            g_Player.left = (int16_t)(40 + (frame * 7) % 550);
            g_Session.frame_rand++;
            phases |= (uint16_t)(1u << m_B10M.status.phase);
            if (m_B10M.status.phase == B10M_PHASE_STAR_ATTACK) {
                star_phases |= (uint16_t)(1u << m_B10M.pentagram.phase);
            }
            const int8_t previous = m_B10M.status.phase;
            if (m_B10M.status.phase == B10M_PHASE_STAR_ATTACK) {
                previous_star_phase = m_B10M.pentagram.phase;
            }
            CHECK(M_BossUpdate(&f.assets, &f.miko));
            if (previous == B10M_PHASE_STAR_ATTACK
                && m_B10M.status.phase == B10M_PHASE_TO_STAR) {
                CHECK_EQ_INT(
                    m_B10M.pentagram.phase,
                    loops == 0 ? B10M_STAR_RETURN : B10M_STAR_WAIT);
                CHECK_EQ_INT(previous_star_phase, loops == 0 ? 6 : 9);
                loops++;
            }
        }
        CHECK_EQ_INT(phases, 0x3fff);
        CHECK_EQ_INT(star_phases, 0x3ff);
        CHECK_EQ_INT(loops, 2);
        CHECK_EQ_INT(m_B10M.status.hp, 16);
        CHECK_EQ_INT(arena.used, used);
        CHECK(g_Rng != 0xb10a0000);
        CHECK(!m_B10M.finished);
        CHECK(m_Scene.active == nullptr);
        Arena_Free(&arena);
    }
}

TEST(missile_variation_consumes_rng_with_a_full_pool)
{
    ARENA arena;
    Arena_Init(&arena, "b10m", 2 * 1024 * 1024);
    FIXTURE f;
    M_Battle(&f, &arena);
    g_BossYuugenMagan.init();
    M_BossSetup(&f.assets, &f.miko);
    m_B10M.status.phase = B10M_PHASE_MISSILES;
    m_B10M.status.frame = 99;
    for (int32_t i = 0; i < MISSILES_COUNT; i++) {
        m_Missiles.slots[i] = (MISSILE) { .left = 200 * 16,
                                          .top = 200 * 16,
                                          .previous_left = -1,
                                          .flag = MISSILE_MOVING };
    }
    uint32_t expected = g_Rng;
    const int16_t variation = (int16_t)(MasterRng_Next(&expected) % 3);
    VSyncIrq_Reset();
    CHECK(M_BossUpdate(&f.assets, &f.miko));
    CHECK_EQ_INT(m_B10M.subphase, variation * 4 + 1);
    CHECK_EQ_INT(g_Rng, expected);
    m_B10M.status.frame = 103;
    CHECK(M_BossUpdate(&f.assets, &f.miko));
    for (int32_t i = 0; i < MISSILES_COUNT; i++) {
        CHECK_EQ_INT(m_Missiles.slots[i].left, 200 * 16);
        CHECK_EQ_INT(m_Missiles.slots[i].top, 200 * 16);
    }
    CHECK_EQ_INT(g_Rng, expected);
    Arena_Free(&arena);
}

TEST(defeat_suspends_prefix_and_activates_the_real_scene)
{
    for (int32_t variant = 0; variant < 3; variant++) {
        const int32_t phase =
            variant == 0 ? B10M_PHASE_STAR_SPAWN : B10M_PHASE_STAR_ATTACK;
        ARENA arena;
        Arena_Init(&arena, "b10m", 2 * 1024 * 1024);
        FIXTURE f;
        M_Battle(&f, &arena);
        g_BossYuugenMagan.init();
        M_BossSetup(&f.assets, &f.miko);
        m_B10M.status.phase = (int8_t)phase;
        m_B10M.status.hp = 0;
        m_B10M.pattern_interval = 24;
        m_B10M.iterations = 1;
        if (variant == 2) {
            m_B10M.pentagram.phase = B10M_STAR_SLAM;
            m_B10M.pentagram.center.y = 420;
            m_B10M.status.frame = 3;
        }
        m_Missiles.slots[0] = (MISSILE) { .flag = MISSILE_MOVING,
                                          .left = 200 * 16,
                                          .top = 200 * 16,
                                          .previous_left = -1,
                                          .velocity = { 16, 0 } };
        g_Pellets.slots[0] =
            (PELLET) { .moving = true, .left = 200 * 16, .top = 200 * 16 };
        g_Pellets.slots[1].cloud_frame = 10;
        m_Lasers.slots[6].alive = true;
        VSyncIrq_Reset();
        CHECK(!M_BossUpdate(&f.assets, &f.miko));
        CHECK(m_B10M.defeating);
        CHECK(!g_Stage.cleared);
        CHECK(m_Scene.active == nullptr);
        CHECK_EQ_INT(m_Missiles.slots[0].left, 201 * 16);
        CHECK_EQ_INT(
            m_Missiles.slots[0].flag,
            phase == B10M_PHASE_STAR_ATTACK ? MISSILE_FREE : MISSILE_MOVING);
        CHECK(!g_Pellets.slots[0].moving);
        CHECK_EQ_INT(g_Pellets.slots[1].cloud_frame, 10);
        CHECK(m_Lasers.slots[6].alive);
        const B10M frozen = m_B10M;
        CHECK(!M_BossUpdate(&f.assets, &f.miko));
        CHECK(memcmp(&m_B10M, &frozen, sizeof(m_B10M)) == 0);
        const size_t used = arena.used;
        for (int32_t frame = 0; frame < 400 && !m_B10M.finished; frame++) {
            VSyncIrq_Edge();
            M_BossUpdate(&f.assets, &f.miko);
        }
        CHECK(m_B10M.finished);
        CHECK(g_Stage.cleared);
        CHECK(g_Player.is_hit);
        CHECK(m_Scene.active == &f.assets.next_scene);
        CHECK_EQ_INT(m_Scene.active->id, 3);
        CHECK_EQ_INT(m_Missiles.slots[0].left, 201 * 16);
        CHECK_EQ_INT(arena.used, used);
        const B10M finished = m_B10M;
        CHECK(M_BossUpdate(&f.assets, &f.miko));
        CHECK(memcmp(&m_B10M, &finished, sizeof(m_B10M)) == 0);
        Arena_Free(&arena);
    }
}

TEST(real_orb_hits_score_and_cross_every_hp_gate)
{
    const int8_t phases[] = {
        B10M_PHASE_LATERAL,        B10M_PHASE_MISSILES, B10M_PHASE_SPRAY,
        B10M_PHASE_MISSILES_CLOCK, B10M_PHASE_LASERS,
    };
    const int16_t hp[] = { 16, 13, 11, 9, 3 };
    for (int32_t i = 0; i < 5; i++) {
        ARENA arena;
        Arena_Init(&arena, "b10m", 2 * 1024 * 1024);
        FIXTURE f;
        M_Battle(&f, &arena);
        g_BossYuugenMagan.init();
        M_BossSetup(&f.assets, &f.miko);
        m_B10M.status.phase = phases[i];
        m_B10M.status.hp = hp[i];
        m_B10M.pattern_interval = 24;
        m_B10M.eyes[0].image = 3;
        m_B10M.eyes[0].orb_inactive = false;
        g_Orb.previous_left = 88;
        g_Orb.previous_top = 148;
        g_Orb.velocity_x = ORB_X_LEFT_4;
        VSyncIrq_Reset();
        CHECK(M_BossUpdate(&f.assets, &f.miko));
        CHECK_EQ_INT(m_B10M.status.hp, hp[i] - 1);
        CHECK_EQ_INT(m_B10M.status.phase, phases[i] + 1);
        CHECK_EQ_INT(g_Stage.score, 5000);
        CHECK_EQ_INT(g_Orb.velocity_x, ORB_X_RIGHT_4);
        CHECK(m_B10M.hit.invincible);
        CHECK_EQ_INT(m_B10M.status.frame, 0);
        CHECK_EQ_INT(m_B10M.hit.frame, 0);
        const int16_t old_hp = m_B10M.status.hp;
        CHECK(M_BossUpdate(&f.assets, &f.miko));
        CHECK_EQ_INT(m_B10M.status.hp, old_hp);
        CHECK_EQ_INT(g_Stage.score, 5000);
        Arena_Free(&arena);
    }
}

TEST(entrance_white_lines_keep_asymmetric_lethal_bounds)
{
    const int16_t positions[] = { 70, 90, 550, 560 };
    for (int32_t i = 0; i < 4; i++) {
        ARENA arena;
        Arena_Init(&arena, "b10m", 2 * 1024 * 1024);
        FIXTURE f;
        M_Battle(&f, &arena);
        g_BossYuugenMagan.init();
        M_BossSetup(&f.assets, &f.miko);
        m_B10M.status.frame = 160;
        m_B10M.hp_bar_filled = true;
        g_Player.invincible = false;
        g_Player.left = positions[i];
        VSyncIrq_Reset();
        CHECK(M_BossUpdate(&f.assets, &f.miko));
        CHECK_EQ_INT(g_Player.is_hit, i == 0 || i == 2);
        CHECK_EQ_INT(Th01Test_Pixel(0, 92, 156), 7);
        CHECK_EQ_INT(Th01Test_Pixel(0, 540, 398), 7);
        CHECK_EQ_INT(Th01Test_Pixel(0, 540, 399), 0);
        g_Player.invincible = true;
        CHECK(M_BossUpdate(&f.assets, &f.miko));
        CHECK(!g_Player.is_hit);
        Arena_Free(&arena);
    }
}

TEST(palette_recolor_keeps_signed_overflow_and_dac_clamp)
{
    ARENA arena;
    Arena_Init(&arena, "b10m", 2 * 1024 * 1024);
    FIXTURE f;
    M_Battle(&f, &arena);
    g_BossYuugenMagan.init();
    M_BossSetup(&f.assets, &f.miko);
    m_B10M.status.phase = B10M_PHASE_STAR_RISE;
    m_B10M.pentagram.center = (MATH16_VECTOR) { 316, 240 };
    m_B10M.pentagram.radius = 64;
    g_StagePalette.colors[15][0] = 126;
    g_StagePalette.colors[15][1] = 0;
    g_StagePalette.colors[15][2] = 5;
    Palette_Set(&g_Palette, &g_StagePalette);
    VSyncIrq_Reset();
    for (int32_t i = 0; i < 70; i++) {
        CHECK(M_BossUpdate(&f.assets, &f.miko));
    }
    CHECK_EQ_INT(g_StagePalette.colors[15][0], -67);
    CHECK_EQ_INT(g_Palette.saved.colors[15][0], 0);
    CHECK_EQ_INT(g_Palette.saved.colors[15][2], 5);
    CHECK_EQ_INT(m_B10M.status.phase, B10M_PHASE_STAR_ATTACK);
    CHECK_EQ_INT(m_B10M.pentagram.center.y, 189);
    CHECK_EQ_INT(m_B10M.pentagram.angle, 0);
    CHECK_EQ_INT(m_B10M.pattern_interval, 0);
    Arena_Free(&arena);
}
