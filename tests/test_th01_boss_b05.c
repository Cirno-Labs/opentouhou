#include "../src/th01/boss_b05.c"
#include "../src/th01/hud_hp.c"
#include "../src/th01/boss_entity.c"
#include "../src/th01/scene.c"
#include "../src/th01/particles.c"
#include "../src/th01/vsync_irq.c"
#include "th01/sound.h"
#include "th01/shots.h"
#include "th01/player.h"
#include "th01/pellets.h"
#include "th01/orb.h"
#include "th01/rng.h"
#include "th01/session.h"

#include "core/base/arena.h"
#include "pc98/zun/master_rng.h"

#include "th01_world.h"
#include "test.h"

static void M_BossSetup(const B05_ASSETS *const assets)
{
    const BOSS_CONTEXT ctx = {};
    g_BossSingyoku.setup(assets, &ctx);
}

static bool M_BossUpdate(
    const PTN *const miko, const ROUTE_SELECT_ASSETS *const route,
    const TH01_INPUT *const raw)
{
    const BOSS_CONTEXT ctx = { .miko = miko, .route = route, .raw = raw };
    return g_BossSingyoku.update(nullptr, &ctx);
}

typedef struct {
    B05_ASSETS assets;
    GAME_TEXT_FONT font;
    PTN stage, miko;
    SCENE_DATA cache[3];
    ROUTE_SELECT_ASSETS route_assets;

} FIXTURE;

static void M_InitFixture(FIXTURE *const f, ARENA *const arena)
{
    *f = (FIXTURE) {};
    Th01Test_ResetWorld();
    GameInput_Init();
    const ALLOCATOR *const allocator = Arena_Allocator(arena);
    Video_Init(allocator);
    const uint8_t *const pixels = Arena_AllocZ(arena, 96 * 96 * 8);
    const uint8_t *const coverage = Arena_AllocZ(arena, 96 * 96 * 8);
    f->assets.sphere = (BOS) { 96, 96, 8, pixels, coverage };
    f->assets.flash = (BOS) { 96, 96, 3, pixels, coverage };
    f->assets.person = (BOS) { 96, 96, 5, pixels, coverage };
    f->assets.background = (PI_IMAGE) {
        .width = 640,
        .height = 400,
        .has_palette = true,
        .pixels = Arena_AllocZ(arena, VIDEO_PAGE_SIZE),
    };
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
        glyphs[i].width = 16;
        for (int32_t y = 0; y < 16; y++) {
            glyphs[i].rows[y] = glyphs[i].codepoint == 0x3000
                ? 0
                : (uint16_t)(0x8000u >> ((i + y) % 16));
        }
    }
    CHECK(Result_Ignore(GameText_LoadGlyphs(glyphs, 96, a, &f->font)));
    f->stage = (PTN) { .count = 4, .pixels = Allocator_Alloc(a, 4096) };
    memset(f->stage.pixels, PTN_TRANSPARENT, 4096);
    f->stage.pixels[3 * 1024] = 7;
    f->miko = (PTN) { .count = 1, .pixels = Allocator_Alloc(a, 1024) };
    memset(f->miko.pixels, PTN_TRANSPARENT, 1024);
    f->miko.pixels[0] = 6;
    for (uint8_t id = 0; id < 3; id++) {
        uint8_t bytes[22 + 5 * 250] = {};
        bytes[22] = (uint8_t)(8 >> id);
        READER reader;
        Reader_OpenBuffer(&reader, bytes, sizeof(bytes), nullptr);
        CHECK(Result_Ignore(Scene_LoadReader(&reader, id, a, &f->cache[id])));
        Reader_Close(&reader);
    }
    Scene_Activate(&f->cache[0]);
    g_Session.route = 1;
    g_Session.lives = 3;
    g_Rng = 0xb0500000;
    g_Player.invincible = true;
    g_Player.left = 300;
    g_Orb.previous_top = 64;
    Hud_Load(nullptr, &f->stage, &f->font);
    f->route_assets = (ROUTE_SELECT_ASSETS) {
        &f->font,
        &f->stage,
        &f->cache[1],
        &f->cache[2],
    };

    g_Session.settings.difficulty = SETTINGS_DIFFICULTY_EASY;
}

static void M_Enter(FIXTURE *const f)
{
    const TH01_INPUT raw = {};
    CHECK(!M_BossUpdate(&f->miko, &f->route_assets, &raw));
    for (int32_t frame = 0; frame < 200; frame++) {
        VSyncIrq_Edge();
        CHECK_EQ_INT(
            M_BossUpdate(&f->miko, &f->route_assets, &raw), frame == 199);
    }
    CHECK_EQ_INT(m_B05.status.phase, B05_PHASE_SPHERE);
    CHECK_EQ_INT(m_B05.status.frame, 0);
}

TEST(setup_binds_real_banks_and_preserves_static_history)
{
    ARENA arena;
    Arena_Init(&arena, "b05", 2 * 1024 * 1024);
    FIXTURE f;
    M_InitFixture(&f, &arena);
    memset(g_Video.pages[0], 3, VIDEO_PAGE_SIZE);
    memset(g_Video.pages[1], 5, VIDEO_PAGE_SIZE);
    const size_t used = arena.used;
    g_BossSingyoku.init();
    m_B05.status.frame = 77;
    m_B05.pattern = 3;
    m_B05.velocity_x = 999;
    m_B05.velocity_y = -4;
    m_B05.angle = 42;
    m_B05.direction = -1;
    m_B05.hp_bar_filled = true;
    m_B05.hit = (BOSS_HIT) { .frame = 23, .invincible = true };
    m_B05.sphere.previous_left = 17;
    m_B05.sphere.lock_frame = 2;
    m_B05.person.image = 5;
    m_Particles.cycle = 73;
    m_Particles.velocity_base[0] = 9;
    m_Particles.alive[0] = 255;
    M_BossSetup(&f.assets);
    CHECK((*g_Boss) == &m_B05.status);
    CHECK_EQ_INT(arena.used, used);
    CHECK(m_Sprites.entities[0] == &f.assets.sphere);
    CHECK(m_Sprites.entities[1] == &f.assets.flash);
    CHECK(m_Sprites.entities[2] == &f.assets.person);
    CHECK_EQ_INT(m_B05.sphere.left, 640);
    CHECK_EQ_INT(m_B05.sphere.top, 64);
    CHECK_EQ_INT(m_B05.sphere.orb_hitbox.left, 24);
    CHECK_EQ_INT(m_B05.sphere.orb_hitbox.right, 72);
    CHECK_EQ_INT(m_B05.sphere.move_clamp.right, 736);
    CHECK_EQ_INT(m_B05.sphere.image, 0);
    CHECK_EQ_INT(m_B05.sphere.previous_left, 17);
    CHECK_EQ_INT(m_B05.sphere.lock_frame, 2);
    CHECK_EQ_INT(m_B05.person.image, 5);
    CHECK_EQ_INT(m_B05.status.phase, B05_PHASE_START);
    CHECK_EQ_INT(m_B05.status.hp, 8);
    CHECK_EQ_INT(m_B05.status.frame, 77);
    CHECK_EQ_INT(m_B05.hit.frame, 23);
    CHECK(m_B05.hit.invincible && m_B05.hp_bar_filled);
    CHECK_EQ_INT(m_B05.pattern, 3);
    CHECK_EQ_INT(m_B05.velocity_x, 999);
    CHECK_EQ_INT(m_B05.direction, -1);
    CHECK_EQ_INT(m_Health.first_white, 6);
    CHECK_EQ_INT(m_Health.first_redwhite, 2);
    CHECK_EQ_INT(m_Particles.alive[0], 0);
    CHECK_EQ_INT(m_Particles.cycle, 73);
    CHECK_EQ_INT(m_Particles.velocity_base[0], 9);
    CHECK(memcmp(&m_B05.boss_palette, &f.assets.palette, sizeof(PALETTE)) == 0);
    CHECK(memcmp(&g_StagePalette, &m_B05.post_defeat, sizeof(PALETTE)) == 0);
    CHECK_EQ_INT(g_Video.pages[0][0], 3);
    CHECK_EQ_INT(g_Video.pages[1][0], 5);
    CHECK_EQ_INT(g_Video.palette[5].red, 5 * 17);
    Arena_Free(&arena);
}

TEST(asset_validation_rejects_incomplete_and_wrong_sized_banks)
{
    ARENA arena;
    Arena_Init(&arena, "b05", 2 * 1024 * 1024);
    FIXTURE f;
    M_InitFixture(&f, &arena);
    CHECK(Result_Ignore(M_ValidateAssets(&f.assets)));
    f.assets.person.count = 4;
    CHECK(!Result_Ignore(M_ValidateAssets(&f.assets)));
    f.assets.person.count = 5;
    f.assets.flash.width = 80;
    CHECK(!Result_Ignore(M_ValidateAssets(&f.assets)));
    f.assets.flash.width = 96;
    f.assets.background.has_palette = false;
    CHECK(!Result_Ignore(M_ValidateAssets(&f.assets)));
    Arena_Free(&arena);
}

TEST(entrance_waits_all_frames_and_leaves_world_untouched)
{
    for (int32_t reuse = 0; reuse < 2; reuse++) {
        ARENA arena;
        Arena_Init(&arena, "b05", 2 * 1024 * 1024);
        FIXTURE f;
        M_Battle(&f, &arena);
        g_BossSingyoku.init();
        M_BossSetup(&f.assets);
        const size_t used = arena.used;
        VSyncIrq_Reset();
        m_VSync.count1 = (uint16_t)reuse;
        m_VSync.count2 = g_VSync->count2;
        const TH01_INPUT raw = {};
        const PLAYER player = g_Player;
        const ORB orb = g_Orb;
        const uint32_t rng = g_Rng;
        CHECK(!M_BossUpdate(&f.miko, &f.route_assets, &raw));
        CHECK_EQ_INT(m_B05.status.frame, 1 + reuse);
        CHECK_EQ_INT(m_B05.hit.frame, 0);
        CHECK_EQ_INT(g_Palette.saved.colors[5][0], 0);
        const B05 frozen = m_B05;
        CHECK(!M_BossUpdate(&f.miko, &f.route_assets, &raw));
        CHECK(memcmp(&m_B05, &frozen, sizeof(m_B05)) == 0);
        for (int32_t edge = 0; edge < 200 - reuse; edge++) {
            VSyncIrq_Edge();
            CHECK_EQ_INT(
                M_BossUpdate(&f.miko, &f.route_assets, &raw),
                edge == 199 - reuse);
        }
        CHECK_EQ_INT(m_B05.status.phase, B05_PHASE_SPHERE);
        CHECK_EQ_INT(m_B05.pattern, 0);
        CHECK_EQ_INT(m_B05.status.frame, 0);
        CHECK_EQ_INT(m_B05.hit.frame, 0);
        CHECK(!m_B05.hp_bar_filled);
        CHECK(memcmp(&g_Player, &player, sizeof(player)) == 0);
        CHECK(memcmp(&g_Orb, &orb, sizeof(orb)) == 0);
        CHECK_EQ_INT(g_Rng, rng);
        CHECK(
            memcmp(&g_Palette.saved, &m_B05.boss_palette, sizeof(PALETTE))
            == 0);
        CHECK_EQ_INT(arena.used, used);
        Arena_Free(&arena);
    }
}

TEST(real_hit_controller_reaches_both_cached_routes)
{
    for (int32_t rank = 0; rank < 4; rank++) {
        ARENA arena;
        Arena_Init(&arena, "b05", 2 * 1024 * 1024);
        FIXTURE f;
        M_Battle(&f, &arena);
        g_Session.settings.difficulty = rank;
        g_Session.settings.difficulty = rank;
        g_BossSingyoku.init();
        M_BossSetup(&f.assets);
        VSyncIrq_Reset();
        M_Enter(&f);
        const size_t used = arena.used;
        const TH01_INPUT raw = {};
        bool phase_two = false;
        for (int32_t step = 1;
             step <= 2400 && m_B05.status.phase != B05_PHASE_DEFEATED; step++) {
            const bool hit =
                step == 20 || step == 70 || (step >= 1620 && step % 50 == 20);
            g_Orb.previous_left = hit ? (int16_t)(m_B05.sphere.left + 32) : 0;
            g_Orb.previous_top = hit ? (int16_t)(m_B05.sphere.top + 32) : 64;
            g_Session.frame_rand = (uint32_t)step;
            CHECK(M_BossUpdate(&f.miko, &f.route_assets, &raw));
            phase_two |= m_B05.status.phase == B05_PHASE_TRANSFORMS;
        }
        CHECK(phase_two);
        CHECK_EQ_INT(m_B05.status.phase, B05_PHASE_DEFEATED);
        CHECK_EQ_INT(m_B05.status.hp, 0);
        CHECK_EQ_INT(g_Stage.score, 24000);
        CHECK_EQ_INT(m_B05.pending, B05_IDLE);
        g_Pellets.slots[0] = (PELLET) {
            .moving = true,
            .left = 100 * 16,
            .top = 100 * 16,
            .previous_left = 100 * 16,
            .previous_top = 100 * 16,
        };
        g_Pellets.slots[1] = (PELLET) { .cloud_frame = 7 };
        CHECK(!M_BossUpdate(&f.miko, &f.route_assets, &raw));
        CHECK(!g_Pellets.slots[0].moving);
        CHECK(!g_Pellets.slots[1].moving);
        CHECK_EQ_INT(g_Pellets.slots[1].cloud_frame, 7);
        CHECK_EQ_INT(m_Scene.active->id, 0);
        CHECK(!g_Stage.cleared && !g_Player.is_hit);
        const B05 frozen = m_B05;
        CHECK(!M_BossUpdate(&f.miko, &f.route_assets, &raw));
        CHECK(memcmp(&m_B05, &frozen, sizeof(m_B05)) == 0);
        const uint32_t rng = g_Rng;
        const uint32_t frame_rand = g_Session.frame_rand;
        for (int32_t edge = 0; edge < 1000 && m_B05.pending != B05_FINISHED;
             edge++) {
            const TH01_INPUT held = { .shot = true, .down = rank % 2 == 1 };
            VSyncIrq_Edge();
            M_BossUpdate(&f.miko, &f.route_assets, &held);
        }
        CHECK_EQ_INT(m_B05.pending, B05_FINISHED);
        CHECK_EQ_INT(g_Session.route, rank % 2);
        CHECK(m_Scene.active == &f.cache[1 + rank % 2]);
        CHECK(g_Stage.cleared && g_Player.is_hit);
        CHECK_EQ_INT(g_Rng, rng);
        CHECK_EQ_INT(g_Session.frame_rand, frame_rand);
        CHECK_EQ_INT(arena.used, used);
        const B05 finished = m_B05;
        CHECK(M_BossUpdate(&f.miko, &f.route_assets, &raw));
        CHECK(memcmp(&m_B05, &finished, sizeof(m_B05)) == 0);
        Arena_Free(&arena);
    }
}

TEST(full_pool_consumes_male_rng_and_crossing_fires_late)
{
    ARENA arena;
    Arena_Init(&arena, "b05", 2 * 1024 * 1024);
    FIXTURE f;
    M_Battle(&f, &arena);
    g_BossSingyoku.init();
    M_BossSetup(&f.assets);
    m_B05.status.phase = B05_PHASE_TRANSFORMS;
    m_B05.sphere.left = 272;
    m_B05.sphere.top = 96;
    m_B05.pattern = 1;
    m_B05.status.frame = 134;
    for (int32_t i = 0; i < PELLETS_COUNT; i++) {
        g_Pellets.slots[i].moving = true;
    }
    const PELLETS full = g_Pellets;
    uint32_t expected = g_Rng;
    for (int32_t i = 0; i < 10; i++) {
        MasterRng_Next(&expected);
    }
    const TH01_INPUT raw = {};
    VSyncIrq_Reset();
    CHECK(M_BossUpdate(&f.miko, &f.route_assets, &raw));
    CHECK_EQ_INT(g_Rng, expected);
    CHECK(memcmp(&g_Pellets, &full, sizeof(full)) == 0);
    m_B05.status.frame = 159;
    CHECK(M_BossUpdate(&f.miko, &f.route_assets, &raw));
    CHECK_EQ_INT(m_B05.person.image, 5);
    m_B05.status.frame = 163;
    CHECK(M_BossUpdate(&f.miko, &f.route_assets, &raw));
    CHECK_EQ_INT(m_B05.person.image, 5);
    m_B05.pattern = 3;
    m_B05.status.frame = 184;
    expected = g_Rng;
    for (int32_t i = 0; i < 20; i++) {
        MasterRng_Next(&expected);
    }
    CHECK(M_BossUpdate(&f.miko, &f.route_assets, &raw));
    CHECK_EQ_INT(g_Rng, expected);
    memset(&g_Pellets, 0, sizeof(g_Pellets));
    m_B05.pattern = 2;
    for (int32_t i = 0; i < 3; i++) {
        m_B05.status.frame = (int16_t)(239 + i * 8);
        CHECK(M_BossUpdate(&f.miko, &f.route_assets, &raw));
        CHECK(g_Pellets.slots[i * 2].moving);
        CHECK(g_Pellets.slots[i * 2 + 1].moving);
    }
    Arena_Free(&arena);
}
