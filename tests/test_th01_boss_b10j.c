#include "../src/th01/boss_b10j.c"
#include "../src/th01/hud_hp.c"
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
#include "th01_world.h"
#include "test.h"
static void M_BossSetup(const B10J_ASSETS *const assets, const PTN *const miko)
{
    const BOSS_CONTEXT ctx = { .miko = miko };
    g_BossMima.setup(assets, &ctx);
}

static bool M_BossResumeSetup(
    const B10J_ASSETS *const assets, const PTN *const miko)
{
    const BOSS_CONTEXT ctx = { .miko = miko };
    return g_BossMima.resume_setup(assets, &ctx);
}

static bool M_BossUpdate(const B10J_ASSETS *const assets, const PTN *const miko)
{
    const BOSS_CONTEXT ctx = { .miko = miko };
    return g_BossMima.update(assets, &ctx);
}

typedef struct {
    B10J_ASSETS assets;
    GAME_TEXT_FONT font;
    PTN stage, miko;
} FIXTURE;

static void M_InitFixture(FIXTURE *const f, ARENA *const arena)
{
    *f = (FIXTURE) {};
    Th01Test_ResetWorld();
    const ALLOCATOR *const a = Arena_Allocator(arena);
    Video_Init(a);
    f->assets.still = (BOS) { 128, 160, 1, Arena_AllocZ(arena, 128 * 160),
                              Arena_AllocZ(arena, 128 * 160) };
    f->assets.animation = (BOS) { 128, 64, 5, Arena_AllocZ(arena, 128 * 64 * 5),
                                  Arena_AllocZ(arena, 128 * 64 * 5) };
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
        Result_Ignore(Scene_LoadReader(&reader, 4, a, &f->assets.next_scene)));
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
    g_Rng = 0xb10b0000;
    g_Player.invincible = true;
    g_Player.left = 300;
    g_Orb.top = 64;
    g_Orb.previous_top = 64;
    Hud_Load(nullptr, &f->stage, &f->font);
    g_Session.settings.difficulty = SETTINGS_DIFFICULTY_EASY;
}

TEST(setup_preserves_saved_palette_and_all_sixteen_waits)
{
    ARENA arena;
    Arena_Init(&arena, "b10j", 2 * 1024 * 1024);
    FIXTURE f;
    M_InitFixture(&f, &arena);
    memset(&m_B10J, 0xa5, sizeof(m_B10J));
    g_BossMima.init();
    CHECK(m_B10J.meteor_active);
    CHECK_EQ_INT(m_B10J.spread_interval, 4);
    CHECK_EQ_INT(m_B10J.spread_speed, 8);
    m_B10J.status.frame = 91;
    m_B10J.status.hp = 3;
    m_B10J.still.lock_frame = 2;
    m_B10J.still.previous_top = 27;
    m_B10J.hit.frame = 17;
    m_B10J.pattern_state = 99;
    m_Missiles.slots[0].flag = MISSILE_MOVING;
    m_Missiles.slots[0].previous_top = 36;
    VSyncIrq_Reset();
    g_VSync.count1 = 12;
    g_VSync.count2 = 19;
    M_BossSetup(&f.assets, &f.miko);
    CHECK(g_Boss == &m_B10J.status);
    const size_t used = arena.used;
    CHECK(!M_BossResumeSetup(&f.assets, &f.miko));
    CHECK_EQ_INT(m_B10J.status.frame, 91);
    CHECK_EQ_INT(m_B10J.status.hp, 3);
    for (int32_t step = 0; step < 16; step++) {
        VSyncIrq_Edge();
        if (step == 15) {
            CHECK(M_BossResumeSetup(&f.assets, &f.miko));
        } else {
            CHECK(!M_BossResumeSetup(&f.assets, &f.miko));
            const B10J frozen = m_B10J;
            CHECK(!M_BossResumeSetup(&f.assets, &f.miko));
            CHECK(memcmp(&m_B10J, &frozen, sizeof(m_B10J)) == 0);
            CHECK_EQ_INT(m_B10J.status.hp, 3);
        }
        CHECK(
            memcmp(&g_Palette.saved, &f.assets.palette, sizeof(PALETTE)) == 0);
        for (int32_t i = 0; i < 16; i++) {
            const int32_t target = f.assets.palette.colors[i][0];
            const int32_t faded = 14 - step;
            CHECK_EQ_INT(
                g_Video.palette[i].red, (faded > target ? faded : target) * 17);
        }
    }
    CHECK_EQ_INT(m_B10J.still.left, 256);
    CHECK_EQ_INT(m_B10J.still.top, 64);
    CHECK_EQ_INT(m_B10J.still.previous_top, 27);
    CHECK_EQ_INT(m_B10J.still.lock_frame, 2);
    CHECK_EQ_INT(m_B10J.hit.frame, 17);
    CHECK_EQ_INT(m_B10J.pattern_state, 99);
    CHECK_EQ_INT(m_B10J.status.hp, 12);
    CHECK_EQ_INT(m_B10J.status.frame, 0);
    CHECK_EQ_INT(m_B10J.status.phase, B10J_PHASE_START);
    CHECK_EQ_INT(m_Health.first_white, 6);
    CHECK_EQ_INT(m_Health.first_redwhite, 2);
    CHECK_EQ_INT(m_Missiles.slots[0].flag, MISSILE_FREE);
    CHECK_EQ_INT(m_Missiles.slots[0].previous_top, 36);
    CHECK_EQ_INT(g_VSync.count1, 28);
    CHECK_EQ_INT(g_VSync.count2, 35);
    CHECK_EQ_INT(arena.used, used);
    Arena_Free(&arena);
}

TEST(assets_require_both_banks_and_the_next_scene)
{
    ARENA arena;
    Arena_Init(&arena, "b10j", 2 * 1024 * 1024);
    FIXTURE f;
    M_InitFixture(&f, &arena);
    CHECK(Result_Ignore(M_ValidateAssets(&f.assets)));
    f.assets.animation.count = 4;
    CHECK(!Result_Ignore(M_ValidateAssets(&f.assets)));
    f.assets.animation.count = 5;
    f.assets.next_scene.id = 3;
    CHECK(!Result_Ignore(M_ValidateAssets(&f.assets)));
    Arena_Free(&arena);
}

static void M_SetupFixture(FIXTURE *const f)
{
    g_BossMima.init();
    M_BossSetup(&f->assets, &f->miko);
    while (!M_BossResumeSetup(&f->assets, &f->miko)) {
        VSyncIrq_Edge();
    }
}

static void M_Enter(FIXTURE *const f)
{
    for (int32_t edge = 0; edge < 110; edge++) {
        if (M_BossUpdate(&f->assets, &f->miko)) {
            CHECK_EQ_INT(m_B10J.status.phase, B10J_PHASE_FIRST);
            CHECK_EQ_INT(m_B10J.status.frame, 0);
            return;
        }
        VSyncIrq_Edge();
    }
    TEST_FAIL("entrance did not complete");
}

TEST(entrance_preserves_prefix_and_requires_every_delay)
{
    ARENA arena;
    Arena_Init(&arena, "b10j", 2 * 1024 * 1024);
    FIXTURE f;
    M_Battle(&f, &arena);
    VSyncIrq_Reset();
    M_SetupFixture(&f);
    g_VSync.count1 = 0;
    m_Missiles.slots[0] = (MISSILE) { .flag = MISSILE_MOVING,
                                      .left = 200 * 16,
                                      .top = 200 * 16,
                                      .previous_left = -1,
                                      .velocity = { 16, 0 } };
    const size_t used = arena.used;
    CHECK(!M_BossUpdate(&f.assets, &f.miko));
    CHECK_EQ_INT(m_Missiles.slots[0].left, 201 * 16);
    CHECK_EQ_INT(m_B10J.status.frame, 1);
    const B10J frozen = m_B10J;
    const uint32_t frozen_rng = g_Rng;
    CHECK(!M_BossUpdate(&f.assets, &f.miko));
    CHECK(memcmp(&m_B10J, &frozen, sizeof(m_B10J)) == 0);
    CHECK_EQ_INT(g_Rng, frozen_rng);
    for (int32_t edge = 1; edge <= 100; edge++) {
        VSyncIrq_Edge();
        const bool done = M_BossUpdate(&f.assets, &f.miko);
        CHECK(done == (edge == 100));
    }
    CHECK_EQ_INT(m_B10J.status.phase, B10J_PHASE_FIRST);
    CHECK_EQ_INT(m_B10J.status.frame, 0);
    CHECK_EQ_INT(m_B10J.still.left, 256);
    CHECK_EQ_INT(m_B10J.still.top, 120);
    CHECK_EQ_INT(m_Missiles.slots[0].left, 201 * 16);
    uint32_t expected = frozen_rng;
    MasterRng_Next(&expected);
    CHECK_EQ_INT(g_Rng, expected);
    CHECK_EQ_INT(m_B10J.hop, 255);
    CHECK_EQ_INT(m_B10J.spread_interval, 4);
    CHECK_EQ_INT(m_B10J.spread_speed, 16);
    CHECK_EQ_INT(arena.used, used);
    Arena_Free(&arena);
}

TEST(all_ranks_complete_eight_patterns_without_allocating)
{
    for (int32_t rank = 0; rank < 4; rank++) {
        ARENA arena;
        Arena_Init(&arena, "b10j", 2 * 1024 * 1024);
        FIXTURE f;
        M_Battle(&f, &arena);
        g_Session.settings.difficulty = rank;
        g_Session.settings.difficulty = rank;
        VSyncIrq_Reset();
        M_SetupFixture(&f);
        M_Enter(&f);
        const size_t used = arena.used;
        uint16_t patterns = 0;
        int32_t loops = 0;
        for (int32_t step = 0; step < 12000 && loops < 2; step++) {
            const int8_t phase = m_B10J.status.phase;
            const int16_t pattern = m_B10J.pattern;
            if ((phase == B10J_PHASE_FIRST || phase == B10J_PHASE_SECOND)
                && pattern >= 0 && pattern < 4) {
                patterns |=
                    (uint16_t)(1u
                               << (pattern
                                   + (phase == B10J_PHASE_SECOND ? 4 : 0)));
            }
            if ((patterns & 15) == 15) {
                m_B10J.status.hp = 6;
            }
            g_Session.frame_rand++;
            CHECK(M_BossUpdate(&f.assets, &f.miko));
            if (phase == B10J_PHASE_SECOND && pattern == 3
                && m_B10J.pattern == 0) {
                loops++;
            }
        }
        CHECK_EQ_INT(patterns, 255);
        CHECK_EQ_INT(loops, 2);
        CHECK_EQ_INT(m_B10J.status.hp, 6);
        CHECK(!m_B10J.finished);
        CHECK(m_Scene.active == nullptr);
        CHECK_EQ_INT(arena.used, used);
        Arena_Free(&arena);
    }
}

TEST(real_orb_hits_gate_then_finish_the_fight)
{
    ARENA arena;
    Arena_Init(&arena, "b10j", 2 * 1024 * 1024);
    FIXTURE f;
    M_Battle(&f, &arena);
    VSyncIrq_Reset();
    M_SetupFixture(&f);
    M_Enter(&f);
    const size_t used = arena.used;
    bool saw_gate = false;
    for (int32_t step = 0; step < 5000 && !m_B10J.defeating; step++) {
        g_Session.frame_rand++;
        const bool colliding = step % 50 == 20
            && (m_B10J.status.phase == B10J_PHASE_SECOND
                || (m_B10J.status.phase == B10J_PHASE_FIRST
                    && m_B10J.status.hp > 6));
        g_Orb.previous_left =
            colliding ? Math16_Wrap(m_B10J.still.left + 48) : 0;
        g_Orb.previous_top =
            colliding ? Math16_Wrap(m_B10J.still.top + 48) : 64;
        if (m_B10J.status.hp <= 6 && m_B10J.status.phase == B10J_PHASE_FIRST) {
            saw_gate = true;
        }
        M_BossUpdate(&f.assets, &f.miko);
    }
    CHECK(saw_gate);
    CHECK(m_B10J.defeating);
    CHECK_EQ_INT(m_B10J.status.hp, 0);
    CHECK_EQ_INT(g_Stage.score, 60000);
    const B10J frozen = m_B10J;
    const uint32_t rng = g_Rng;
    CHECK(!M_BossUpdate(&f.assets, &f.miko));
    CHECK(memcmp(&m_B10J, &frozen, sizeof(m_B10J)) == 0);
    CHECK_EQ_INT(g_Rng, rng);
    for (int32_t edge = 0; edge < 400 && !m_B10J.finished; edge++) {
        VSyncIrq_Edge();
        M_BossUpdate(&f.assets, &f.miko);
    }
    CHECK(m_B10J.finished);
    CHECK(g_Stage.cleared);
    CHECK(g_Player.is_hit);
    CHECK(m_Scene.active == &f.assets.next_scene);
    CHECK_EQ_INT(m_Scene.active->id, 4);
    const B10J finished = m_B10J;
    CHECK(M_BossUpdate(&f.assets, &f.miko));
    CHECK(memcmp(&m_B10J, &finished, sizeof(m_B10J)) == 0);
    CHECK_EQ_INT(arena.used, used);
    Arena_Free(&arena);
}

TEST(two_squares_preserve_cloud_spawns_and_laser_slots)
{
    ARENA arena;
    Arena_Init(&arena, "b10j", 2 * 1024 * 1024);
    FIXTURE f;
    M_Battle(&f, &arena);
    VSyncIrq_Reset();
    M_SetupFixture(&f);
    M_Enter(&f);
    m_B10J.status.phase = B10J_PHASE_FIRST;
    m_B10J.pattern = 2;
    m_B10J.status.frame = 191;
    m_B10J.squares[2] = (B10J_SQUARE) { .angle = 32, .radius = 112 };
    CHECK(M_BossUpdate(&f.assets, &f.miko));
    for (int32_t i = 0; i < 8; i++) {
        CHECK(g_Pellets.slots[i].cloud_frame != 0);
    }
    CHECK(!g_Pellets.spawn_with_cloud);
    m_B10J.status.phase = B10J_PHASE_SECOND;
    m_B10J.pattern = 3;
    m_B10J.status.frame = 180;
    m_B10J.pattern_state = 50;
    m_B10J.laser_x[1] = 200;
    m_B10J.laser_y[1] = 128;
    CHECK(M_BossUpdate(&f.assets, &f.miko));
    CHECK(m_Lasers.slots[1].alive);
    CHECK_EQ_INT(m_Lasers.slots[1].origin_left, 200 * 256);
    const SHOOTOUT_LASER first = m_Lasers.slots[1];
    m_B10J.status.frame = 184;
    m_B10J.laser_x[1] = 300;
    CHECK(M_BossUpdate(&f.assets, &f.miko));
    CHECK(memcmp(&first, &m_Lasers.slots[1], sizeof(first)) == 0);
    Arena_Free(&arena);
}

TEST(hp_gate_waits_for_pattern_cleanup_and_invincibility)
{
    ARENA arena;
    Arena_Init(&arena, "b10j", 2 * 1024 * 1024);
    FIXTURE f;
    M_Battle(&f, &arena);
    VSyncIrq_Reset();
    M_SetupFixture(&f);
    M_Enter(&f);
    m_B10J.status.hp = 6;
    m_B10J.pattern = 0;
    m_B10J.status.frame = 360;
    m_B10J.hp_bar_filled = true;
    m_B10J.hit = (BOSS_HIT) { .frame = 0, .invincible = true };
    CHECK(M_BossUpdate(&f.assets, &f.miko));
    CHECK_EQ_INT(m_B10J.status.phase, B10J_PHASE_FIRST);
    CHECK_EQ_INT(m_B10J.pattern, 99);
    CHECK_EQ_INT(m_B10J.status.frame, 0);
    for (int32_t i = 0; i < 39; i++) {
        CHECK(M_BossUpdate(&f.assets, &f.miko));
        CHECK_EQ_INT(m_B10J.status.phase, B10J_PHASE_FIRST);
    }
    CHECK(M_BossUpdate(&f.assets, &f.miko));
    CHECK_EQ_INT(m_B10J.status.phase, B10J_PHASE_SPREAD);
    CHECK_EQ_INT(m_B10J.pattern, 0);
    CHECK_EQ_INT(m_B10J.hit.frame, 0);
    CHECK_EQ_INT(m_B10J.spread_interval, 4);
    CHECK_EQ_INT(m_B10J.spread_speed, 8);
    Arena_Free(&arena);
}

TEST(defeat_keeps_clouds_and_resets_only_five_lasers)
{
    ARENA arena;
    Arena_Init(&arena, "b10j", 2 * 1024 * 1024);
    FIXTURE f;
    M_Battle(&f, &arena);
    VSyncIrq_Reset();
    M_SetupFixture(&f);
    M_Enter(&f);
    m_B10J.status.phase = B10J_PHASE_SECOND;
    m_B10J.pattern = 1;
    m_B10J.status.frame = 0;
    m_B10J.status.hp = 0;
    m_Missiles.slots[0] = (MISSILE) { .flag = MISSILE_MOVING,
                                      .left = 200 * 16,
                                      .top = 200 * 16,
                                      .previous_left = -1,
                                      .previous_top = 88,
                                      .velocity = { 16, 0 } };
    g_Pellets.slots[0] =
        (PELLET) { .moving = true, .left = 200 * 16, .top = 200 * 16 };
    g_Pellets.slots[1].cloud_frame = 10;
    for (int32_t i = 0; i < 10; i++) {
        m_Lasers.slots[i].alive = true;
    }
    CHECK(!M_BossUpdate(&f.assets, &f.miko));
    CHECK(m_B10J.defeating);
    CHECK_EQ_INT(m_Missiles.slots[0].left, 201 * 16);
    CHECK_EQ_INT(m_Missiles.slots[0].flag, MISSILE_FREE);
    CHECK(!g_Pellets.slots[0].moving);
    CHECK_EQ_INT(g_Pellets.slots[1].cloud_frame, 10);
    for (int32_t i = 0; i < 10; i++) {
        CHECK(m_Lasers.slots[i].alive == (i >= 5));
    }
    CHECK(!g_Stage.cleared);
    CHECK(m_Scene.active == nullptr);
    Arena_Free(&arena);
}

TEST(entrance_reuses_the_live_interrupt_counter)
{
    const int16_t initial[] = { 1, 12 };
    for (int32_t run = 0; run < 2; run++) {
        ARENA arena;
        Arena_Init(&arena, "b10j", 2 * 1024 * 1024);
        FIXTURE f;
        M_Battle(&f, &arena);
        VSyncIrq_Reset();
        M_SetupFixture(&f);
        g_VSync.count1 = initial[run];
        g_VSync.count2 = 33;
        int32_t edges = 0;
        while (!M_BossUpdate(&f.assets, &f.miko) && edges < 110) {
            VSyncIrq_Edge();
            edges++;
        }
        CHECK_EQ_INT(edges, run == 0 ? 99 : 98);
        CHECK_EQ_INT(g_VSync.count1, 0);
        CHECK_EQ_INT(g_VSync.count2, 33 + edges);
        CHECK_EQ_INT(m_B10J.status.phase, B10J_PHASE_FIRST);
        CHECK_EQ_INT(m_B10J.status.frame, 0);
        Arena_Free(&arena);
    }
}
