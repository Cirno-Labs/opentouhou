#include "tests/test.h"
#include "tests/th01/th01_text_fixture.h"
#include "tests/th01/th01_world.h"
#include "th01/boss/b15j.c"
#include "th01/boss/boss.c"
#include "th01/boss/entity.c"
#include "th01/hud/hp.c"
#include "th01/orb.c"
#include "th01/orb.h"
#include "th01/pellets.h"
#include "th01/pellets/pellets.c"
#include "th01/player_private.h"
#include "th01/rng.h"
#include "th01/scene.c"
#include "th01/session_private.h"
#include "th01/shootout_lasers.c"
#include "th01/shots.h"
#include "th01/sound.h"
#include "th01/vsync_irq.c"

#include "core/base/arena.h"
#include "core/io/reader.h"
#include "pc98/zun/master_rng.h"
#include "pc98/zun/math16.h"

#include <string.h>

static void M_BossSetup(const B15J_ASSETS *const assets, const PTN *const miko)
{
    const BOSS_CONTEXT ctx = { .miko = miko };
    g_BossKikuri.setup(assets, &ctx);
}

static bool M_BossUpdate(
    const B15J_ASSETS *const assets, const PTN *const miko,
    const PTN *const stage)
{
    const BOSS_CONTEXT ctx = { .miko = miko, .stage = stage };
    return g_BossKikuri.update(assets, &ctx);
}

typedef struct {
    B15J_ASSETS assets;
    GAME_TEXT_FONT font;
    PTN stage;
    PTN miko;
} FIXTURE;

static void M_InitFixture(FIXTURE *const f, ARENA *const arena)
{
    *f = (FIXTURE) {};
    Th01Test_ResetWorld();
    const ALLOCATOR *const a = Arena_Allocator(arena);
    Video_Init(a);
    f->assets.souls = (BOS) { 32, 32, 3, Arena_AllocZ(arena, 32 * 32 * 3),
                              Arena_AllocZ(arena, 32 * 32 * 3) };
    f->assets.tears = (BOS) { 16, 16, 3, Arena_AllocZ(arena, 16 * 16 * 3),
                              Arena_AllocZ(arena, 16 * 16 * 3) };
    f->assets.ripples =
        (PTN) { .count = 1, .pixels = Arena_AllocZ(arena, 1024) };
    f->assets.background =
        (PI_IMAGE) { .width = 640,
                     .height = 400,
                     .has_palette = true,
                     .pixels = Arena_AllocZ(arena, VIDEO_PAGE_SIZE) };
    uint8_t bytes[22 + 5 * 250] = {};
    READER reader;
    Reader_OpenBuffer(&reader, bytes, sizeof(bytes), nullptr);
    CHECK(
        Result_Ignore(Scene_LoadReader(&reader, 6, a, &f->assets.next_scene)));
    Reader_Close(&reader);
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
    p_Session->lives = 3;
    g_Rng = 0xb15a0000;
    p_Player->invincible = true;
    p_Player->left = 300;
    m_Orb.top = 64;
    m_Orb.previous_top = 64;
    Hud_Load(nullptr, &f->stage, &f->font);
    p_Session->settings.difficulty = SETTINGS_DIFFICULTY_EASY;
}

TEST(setup_binds_shared_banks_and_keeps_actor_history)
{
    ARENA arena;
    Arena_Init(&arena, "b15j", 2 * 1024 * 1024);
    FIXTURE f;
    M_InitFixture(&f, &arena);
    g_BossKikuri.init();
    m_B15J.hit = (BOSS_HIT) { .frame = 31, .invincible = true };
    m_B15J.pattern_state = 51;
    m_B15J.patterns_done = 9;
    m_B15J.spiral_angle = 255;
    for (int32_t i = 0; i < 2; i++) {
        m_B15J.souls[i].image = i + 1;
        m_B15J.souls[i].lock_frame = 7 + i;
        m_B15J.souls[i].previous_left = 90 + i;
        m_B15J.souls[i].orb_inactive = true;
    }
    for (int32_t i = 0; i < 10; i++) {
        m_B15J.tears[i].left = 100 + i;
        m_B15J.tears[i].image = 2;
        m_B15J.tear_frame[i] = 74;
    }
    const PALETTE saved = g_Palette->saved;
    const size_t used = arena.used;
    M_BossSetup(&f.assets, &f.miko);
    CHECK((*g_Boss) == &m_B15J.status);
    CHECK_EQ_INT(m_B15J.status.hp, 14);
    CHECK_EQ_INT(m_B15J.status.phase, B15J_PHASE_ENTRANCE);
    CHECK_EQ_INT(m_B15J.status.frame, 0);
    CHECK_EQ_INT(m_Health.first_white, 10);
    CHECK_EQ_INT(m_Health.first_redwhite, 6);
    CHECK(m_Sprites.entities[0] == &f.assets.souls);
    CHECK(m_Sprites.entities[1] == &f.assets.tears);
    for (int32_t i = 0; i < 2; i++) {
        CHECK_EQ_INT(m_B15J.souls[i].left, 0);
        CHECK_EQ_INT(m_B15J.souls[i].top, 0);
        CHECK_EQ_INT(m_B15J.souls[i].image, i + 1);
        CHECK_EQ_INT(m_B15J.souls[i].lock_frame, 7 + i);
        CHECK_EQ_INT(m_B15J.souls[i].previous_left, 90 + i);
        CHECK(m_B15J.souls[i].orb_inactive);
        CHECK_EQ_INT(m_B15J.souls[i].move_clamp.left, 32);
        CHECK_EQ_INT(m_B15J.souls[i].move_clamp.right, 576);
        CHECK_EQ_INT(m_B15J.souls[i].move_clamp.top, 64);
        CHECK_EQ_INT(m_B15J.souls[i].move_clamp.bottom, 400);
    }
    for (int32_t i = 0; i < 10; i++) {
        CHECK_EQ_INT(m_B15J.tears[i].left, 100 + i);
        CHECK_EQ_INT(m_B15J.tears[i].image, 2);
        CHECK_EQ_INT(m_B15J.tears[i].sprites.slot, 1);
        CHECK_EQ_INT(m_B15J.tears[i].sprites.count, 3);
        CHECK_EQ_INT(m_B15J.tear_frame[i], 0);
    }
    CHECK(memcmp(&g_Palette->saved, &saved, sizeof(saved)) == 0);
    CHECK(memcmp(&m_B15J.boss_palette, &saved, sizeof(saved)) == 0);
    CHECK_EQ_INT(m_B15J.post_defeat.colors[15][2], 0);
    CHECK_EQ_INT(m_B15J.hit.frame, 31);
    CHECK(m_B15J.hit.invincible);
    CHECK_EQ_INT(m_B15J.pattern_state, 51);
    CHECK_EQ_INT(m_B15J.patterns_done, 9);
    CHECK_EQ_INT(m_B15J.spiral_angle, 255);
    CHECK(m_Pellets.interlace);
    CHECK_EQ_INT(arena.used, used);
    Arena_Free(&arena);
}

TEST(asset_validation_checks_ripples_and_live_banks)
{
    ARENA arena;
    Arena_Init(&arena, "b15j", 2 * 1024 * 1024);
    FIXTURE f;
    M_InitFixture(&f, &arena);
    CHECK(Result_Ignore(M_ValidateAssets(&f.assets)));
    f.assets.ripples.count = 2;
    CHECK(!Result_Ignore(M_ValidateAssets(&f.assets)));
    f.assets.ripples.count = 1;
    f.assets.tears.coverage = nullptr;
    CHECK(!Result_Ignore(M_ValidateAssets(&f.assets)));
    CHECK(!Result_Ignore(M_ValidateAssets(nullptr)));
    Arena_Free(&arena);
}

TEST(loader_failure_clears_output)
{
    ARENA arena;
    Arena_Init(&arena, "b15j", 2 * 1024 * 1024);
    VFS *const vfs = Vfs_Create(Arena_Allocator(&arena));
    B15J_ASSETS out = { .souls = { .count = 3 } };
    CHECK(!Result_Ignore(M_LoadAssets(vfs, Arena_Allocator(&arena), &out)));
    Vfs_Destroy(vfs);
    CHECK_EQ_INT(out.souls.count, 0);
    CHECK(out.souls.pixels == nullptr);
    Arena_Free(&arena);
}

static void M_Enter(FIXTURE *const f)
{
    for (int32_t i = 0; i < 1000; i++) {
        if (M_BossUpdate(&f->assets, &f->miko, &f->stage)) {
            return;
        }
        const B15J frozen = m_B15J;
        CHECK(!M_BossUpdate(&f->assets, &f->miko, &f->stage));
        CHECK(memcmp(&m_B15J, &frozen, sizeof(m_B15J)) == 0);
        VSyncIrq_Edge();
    }
    CHECK(false);
}

TEST(entrance_suspends_without_aging_actors_and_retains_interrupt_history)
{
    for (int32_t page = 0; page < 2; page++) {
        for (int32_t count = 0; count < 2; count++) {
            ARENA arena;
            Arena_Init(&arena, "b15j", 2 * 1024 * 1024);
            FIXTURE f;
            M_Battle(&f, &arena);
            g_BossKikuri.init();
            M_BossSetup(&f.assets, &f.miko);
            Palette_Set(p_Palette, &f.assets.palette);
            Video_SelectDrawingPage(page);
            for (int32_t x = 0; x < 640; x++) {
                for (int32_t y = 0; y < 400; y++) {
                    g_Video->pages[1][y * 640 + x] = (uint8_t)((x + y) % 16);
                }
            }
            const PALETTE saved = g_Palette->saved;
            const size_t used = arena.used;
            VSyncIrq_Reset();
            m_VSync.count1 = count == 0 ? 0 : 17;
            m_VSync.count2 = 21;
            M_Enter(&f);
            CHECK_EQ_INT(m_B15J.status.phase, B15J_PHASE_SPIRAL);
            CHECK_EQ_INT(m_B15J.status.frame, 0);
            CHECK_EQ_INT(m_B15J.status.hp, 14);
            CHECK_EQ_INT(m_B15J.hit.frame, 0);
            CHECK(!m_B15J.hit.invincible);
            CHECK_EQ_INT(g_VSync->edge, count == 0 ? 752 : 751);
            CHECK_EQ_INT(g_VSync->count2, 21 + (int32_t)g_VSync->edge);
            CHECK_EQ_INT(g_Video->drawing_page, 0);
            CHECK(memcmp(&g_Palette->saved, &saved, sizeof(saved)) == 0);
            CHECK(memcmp(p_StagePalette, &saved, sizeof(saved)) == 0);
            CHECK(memcmp(&m_B15J.boss_palette, &saved, sizeof(saved)) == 0);
            CHECK_EQ_INT(m_B15J.souls[0].left, 0);
            CHECK_EQ_INT(m_B15J.souls[1].top, 0);
            CHECK_EQ_INT(arena.used, used);
            Arena_Free(&arena);
        }
    }
}

TEST(all_ranks_run_souls_tears_and_every_late_pattern)
{
    for (int32_t rank = 0; rank < 4; rank++) {
        ARENA arena;
        Arena_Init(&arena, "b15j", 2 * 1024 * 1024);
        FIXTURE f;
        M_Battle(&f, &arena);
        p_Session->settings.difficulty = rank;
        p_Session->settings.difficulty = rank;
        g_BossKikuri.init();
        M_BossSetup(&f.assets, &f.miko);
        VSyncIrq_Reset();
        M_Enter(&f);
        const size_t used = arena.used;
        uint32_t phases = 0;
        uint32_t patterns = 0;
        bool saw_tear = false;
        bool saw_ripple = false;
        for (int32_t frame = 0; frame < 12000; frame++) {
            phases |= 1u << m_B15J.status.phase;
            if (m_B15J.status.phase == B15J_PHASE_PATTERNS) {
                patterns |= 1u << m_B15J.subphase;
            }
            p_Session->frame_rand++;
            CHECK(M_BossUpdate(&f.assets, &f.miko, &f.stage));
            for (int32_t i = 0; i < 10; i++) {
                saw_tear |= m_B15J.tear_frame[i] != 0;
                saw_ripple |= m_B15J.tear_frame[i] > 1;
            }
            ShootoutLasers_UpdateAll(&(SHOOTOUT_LASERS_ASSETS) {});
            VSyncIrq_Edge();
        }
        CHECK_EQ_INT(
            phases,
            (1u << B15J_PHASE_SPIRAL) | (1u << B15J_PHASE_RECOLOR)
                | (1u << B15J_PHASE_SOULS) | (1u << B15J_PHASE_TEARS)
                | (1u << B15J_PHASE_PATTERNS));
        CHECK_EQ_INT(patterns, 15);
        CHECK(saw_tear);
        CHECK(saw_ripple);
        CHECK_EQ_INT(m_B15J.status.hp, 14);
        CHECK(g_Rng != 0xb15a0000);
        CHECK_EQ_INT(arena.used, used);
        CHECK(!p_Player->is_hit);
        Arena_Free(&arena);
    }
}

TEST(current_orb_hitbox_and_hp_gates_reach_real_defeat)
{
    ARENA arena;
    Arena_Init(&arena, "b15j", 2 * 1024 * 1024);
    FIXTURE f;
    M_Battle(&f, &arena);
    g_BossKikuri.init();
    M_BossSetup(&f.assets, &f.miko);
    VSyncIrq_Reset();
    M_Enter(&f);
    const size_t used = arena.used;
    m_Orb.left = 0;
    m_Orb.top = 64;
    m_Orb.previous_left = 300;
    m_Orb.previous_top = 100;
    CHECK(M_BossUpdate(&f.assets, &f.miko, &f.stage));
    CHECK_EQ_INT(m_B15J.status.hp, 14);
    m_Orb.left = 300;
    m_Orb.top = 132;
    CHECK(M_BossUpdate(&f.assets, &f.miko, &f.stage));
    CHECK_EQ_INT(m_B15J.status.hp, 14);
    m_Orb.top = -100;
    m_Orb.previous_left = 0;
    CHECK(M_BossUpdate(&f.assets, &f.miko, &f.stage));
    CHECK_EQ_INT(m_B15J.status.hp, 13);
    CHECK_EQ_INT(g_Stage->score, 7000);
    m_Orb.top = 64;
    m_Pellets.slots[0].cloud_frame = 12;
    m_Lasers.slots[4].alive = true;
    m_Lasers.slots[4].origin_left = 100 * 256;
    for (int32_t frame = 0; frame < 5000 && !m_B15J.finished; frame++) {
        const bool attack =
            (m_B15J.status.phase == B15J_PHASE_SPIRAL && m_B15J.status.hp > 10)
            || (m_B15J.status.phase == B15J_PHASE_TEARS && m_B15J.status.hp > 6)
            || (m_B15J.status.phase == B15J_PHASE_PATTERNS
                && m_B15J.status.hp > 0);
        m_Orb.left = attack ? 300 : 0;
        const bool completed = M_BossUpdate(&f.assets, &f.miko, &f.stage);
        if (!completed) {
            const B15J frozen = m_B15J;
            CHECK(!M_BossUpdate(&f.assets, &f.miko, &f.stage));
            CHECK(memcmp(&m_B15J, &frozen, sizeof(m_B15J)) == 0);
        }
        VSyncIrq_Edge();
    }
    CHECK(m_B15J.finished);
    CHECK_EQ_INT(m_B15J.status.hp, 0);
    CHECK_EQ_INT(g_Stage->score, 98000);
    CHECK(g_Stage->cleared);
    CHECK(p_Player->is_hit);
    CHECK(m_Scene.active == &f.assets.next_scene);
    CHECK_EQ_INT(m_Scene.active->id, 6);
    CHECK_EQ_INT(m_Pellets.slots[0].cloud_frame, 12);
    CHECK(m_Lasers.slots[4].alive);
    const B15J finished = m_B15J;
    CHECK(M_BossUpdate(&f.assets, &f.miko, &f.stage));
    CHECK(memcmp(&m_B15J, &finished, sizeof(m_B15J)) == 0);
    CHECK_EQ_INT(arena.used, used);
    Arena_Free(&arena);
}

TEST(late_rain_reuses_laser_speed_and_full_lasers_still_consume_rng)
{
    for (int32_t rank = 0; rank < 4; rank++) {
        ARENA arena;
        Arena_Init(&arena, "b15j", 2 * 1024 * 1024);
        FIXTURE f;
        M_Battle(&f, &arena);
        p_Session->settings.difficulty = rank;
        p_Session->settings.difficulty = rank;
        g_BossKikuri.init();
        M_BossSetup(&f.assets, &f.miko);
        m_B15J.status.phase = B15J_PHASE_PATTERNS;
        m_B15J.subphase = 1;
        m_B15J.status.frame = 249;
        m_Lasers.slots[0] =
            (SHOOTOUT_LASER) { .alive = true, .age = 77, .origin_left = 3210 };
        uint32_t expected = g_Rng;
        MasterRng_Next(&expected);
        VSyncIrq_Reset();
        CHECK(M_BossUpdate(&f.assets, &f.miko, &f.stage));
        CHECK_EQ_INT(m_B15J.pattern_state, 52 + rank * 4);
        CHECK_EQ_INT(g_Rng, expected);
        CHECK_EQ_INT(m_Lasers.slots[0].age, 77);
        CHECK_EQ_INT(m_Lasers.slots[0].origin_left, 3210);
        m_B15J.subphase = 2;
        m_B15J.status.frame = Math16_Wrap(m_B15J.pattern_state * 2 - 1);
        CHECK(M_BossUpdate(&f.assets, &f.miko, &f.stage));
        int32_t alive = 0;
        for (int32_t i = 0; i < PELLETS_COUNT; i++) {
            alive += m_Pellets.slots[i].moving;
        }
        CHECK_EQ_INT(alive, 12);
        CHECK_EQ_INT(m_B15J.pattern_state, 52 + rank * 4);
        m_B15J.subphase = 3;
        m_B15J.status.frame = 99;
        MasterRng_Next(&expected);
        CHECK(M_BossUpdate(&f.assets, &f.miko, &f.stage));
        CHECK_EQ_INT(g_Rng, expected);
        CHECK_EQ_INT(m_B15J.pattern_state, 60 + rank * 4);
        CHECK_EQ_INT(
            m_B15J.random_range,
            rank == 0       ? 0
                : rank == 1 ? 10
                : rank == 2 ? 16
                            : 20);
        CHECK_EQ_INT(m_Lasers.slots[0].age, 77);
        Arena_Free(&arena);
    }
}

TEST(timeout_forces_one_hp_before_damage_without_a_health_draw)
{
    ARENA arena;
    Arena_Init(&arena, "b15j", 2 * 1024 * 1024);
    FIXTURE f;
    M_Battle(&f, &arena);
    g_BossKikuri.init();
    M_BossSetup(&f.assets, &f.miko);
    m_B15J.status.phase = B15J_PHASE_PATTERNS;
    m_B15J.subphase = 1;
    m_B15J.patterns_done = 30;
    const HUD_HP health = m_Health;
    VSyncIrq_Reset();
    CHECK(M_BossUpdate(&f.assets, &f.miko, &f.stage));
    CHECK_EQ_INT(m_B15J.status.hp, 1);
    CHECK(memcmp(&m_Health, &health, sizeof(health)) == 0);
    CHECK_EQ_INT(g_Stage->score, 0);
    m_TestDamage = true;
    CHECK(!M_BossUpdate(&f.assets, &f.miko, &f.stage));
    CHECK_EQ_INT(m_B15J.status.hp, 0);
    CHECK(!m_TestDamage);
    CHECK(m_B15J.defeating);
    CHECK_EQ_INT(g_Stage->score, 0);
    Arena_Free(&arena);
}

TEST(ripple_collision_keeps_internal_unaligned_coordinates)
{
    const int16_t positions[] = { 20, 21, 37, 38 };
    for (int32_t i = 0; i < 4; i++) {
        ARENA arena;
        Arena_Init(&arena, "b15j", 2 * 1024 * 1024);
        FIXTURE f;
        M_Battle(&f, &arena);
        g_BossKikuri.init();
        M_BossSetup(&f.assets, &f.miko);
        m_B15J.status.phase = B15J_PHASE_PATTERNS;
        m_B15J.subphase = 1;
        m_B15J.tear_frame[0] = 1;
        m_B15J.tears[0].left = 37;
        m_B15J.tears[0].top = 392;
        p_Player->invincible = false;
        p_Player->left = positions[i];
        VSyncIrq_Reset();
        CHECK(M_BossUpdate(&f.assets, &f.miko, &f.stage));
        CHECK_EQ_INT(m_B15J.tear_frame[0], 2);
        CHECK_EQ_INT(p_Player->is_hit, i == 1 || i == 2);
        Arena_Free(&arena);
    }
}

TEST(cloud_policy_is_preserved_when_rings_and_spreads_spawn)
{
    for (int32_t cloud = 0; cloud < 2; cloud++) {
        ARENA arena;
        Arena_Init(&arena, "b15j", 2 * 1024 * 1024);
        FIXTURE f;
        M_Battle(&f, &arena);
        g_BossKikuri.init();
        M_BossSetup(&f.assets, &f.miko);
        m_B15J.status.phase = B15J_PHASE_SOULS;
        m_B15J.subphase = 1;
        m_B15J.status.frame = 79;
        m_B15J.souls[0].left = 100;
        m_B15J.souls[1].left = 500;
        m_B15J.souls[0].top = m_B15J.souls[1].top = 100;
        m_Pellets.spawn_with_cloud = cloud != 0;
        const size_t used = arena.used;
        VSyncIrq_Reset();
        CHECK(M_BossUpdate(&f.assets, &f.miko, &f.stage));
        int32_t count = 0;
        for (int32_t i = 0; i < PELLETS_COUNT; i++) {
            count += cloud != 0 ? m_Pellets.slots[i].cloud_frame == 1
                                : m_Pellets.slots[i].moving;
        }
        CHECK_EQ_INT(count, 12);
        CHECK_EQ_INT(m_Pellets.spawn_with_cloud, cloud != 0);
        CHECK(m_Pellets.interlace);
        CHECK_EQ_INT(arena.used, used);
        Arena_Free(&arena);
    }
}
