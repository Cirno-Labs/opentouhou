#include "../src/th01/hud_hp.c"
#include "../src/th01/boss_entity.c"
#include "../src/th01/shootout_lasers.c"
#include "../src/th01/scene.c"
#include "th01/boss_b15j.h"
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

typedef struct {
    B15J_ASSETS assets;
    GAME_TEXT_FONT font;
    PTN stage, miko;
} FIXTURE;

static void M_Init(FIXTURE *const f, ARENA *const arena)
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
    M_Init(f, arena);
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
    g_Rng = 0xb15a0000;
    g_Player.invincible = true;
    g_Player.left = 300;
    g_Orb.top = 64;
    g_Orb.previous_top = 64;
    Hud_Load(nullptr, &f->stage, &f->font);
    g_Session.settings.difficulty = SETTINGS_DIFFICULTY_EASY;
}

TEST(setup_binds_shared_banks_and_keeps_actor_history)
{
    ARENA arena;
    Arena_Init(&arena, "b15j", 2 * 1024 * 1024);
    FIXTURE f;
    M_Init(&f, &arena);
    B15J_Init();
    g_B15J.hit = (BOSS_HIT) { .frame = 31, .invincible = true };
    g_B15J.pattern_state = 51;
    g_B15J.patterns_done = 9;
    g_B15J.spiral_angle = 255;
    for (int32_t i = 0; i < 2; i++) {
        g_B15J.souls[i].image = i + 1;
        g_B15J.souls[i].lock_frame = 7 + i;
        g_B15J.souls[i].previous_left = 90 + i;
        g_B15J.souls[i].orb_inactive = true;
    }
    for (int32_t i = 0; i < 10; i++) {
        g_B15J.tears[i].left = 100 + i;
        g_B15J.tears[i].image = 2;
        g_B15J.tear_frame[i] = 74;
    }
    const PALETTE saved = g_Palette.saved;
    const size_t used = arena.used;
    B15J_Setup(&f.assets, &f.miko);
    CHECK_EQ_INT(g_B15J.hp, 14);
    CHECK_EQ_INT(g_B15J.phase, B15J_PHASE_ENTRANCE);
    CHECK_EQ_INT(g_B15J.frame, 0);
    CHECK_EQ_INT(m_Health.first_white, 10);
    CHECK_EQ_INT(m_Health.first_redwhite, 6);
    CHECK(m_Sprites.entities[0] == &f.assets.souls);
    CHECK(m_Sprites.entities[1] == &f.assets.tears);
    for (int32_t i = 0; i < 2; i++) {
        CHECK_EQ_INT(g_B15J.souls[i].left, 0);
        CHECK_EQ_INT(g_B15J.souls[i].top, 0);
        CHECK_EQ_INT(g_B15J.souls[i].image, i + 1);
        CHECK_EQ_INT(g_B15J.souls[i].lock_frame, 7 + i);
        CHECK_EQ_INT(g_B15J.souls[i].previous_left, 90 + i);
        CHECK(g_B15J.souls[i].orb_inactive);
        CHECK_EQ_INT(g_B15J.souls[i].move_clamp.left, 32);
        CHECK_EQ_INT(g_B15J.souls[i].move_clamp.right, 576);
        CHECK_EQ_INT(g_B15J.souls[i].move_clamp.top, 64);
        CHECK_EQ_INT(g_B15J.souls[i].move_clamp.bottom, 400);
    }
    for (int32_t i = 0; i < 10; i++) {
        CHECK_EQ_INT(g_B15J.tears[i].left, 100 + i);
        CHECK_EQ_INT(g_B15J.tears[i].image, 2);
        CHECK_EQ_INT(g_B15J.tears[i].sprites.slot, 1);
        CHECK_EQ_INT(g_B15J.tears[i].sprites.count, 3);
        CHECK_EQ_INT(g_B15J.tear_frame[i], 0);
    }
    CHECK(memcmp(&g_Palette.saved, &saved, sizeof(saved)) == 0);
    CHECK(memcmp(&g_B15J.boss_palette, &saved, sizeof(saved)) == 0);
    CHECK_EQ_INT(g_B15J.post_defeat.colors[15][2], 0);
    CHECK_EQ_INT(g_B15J.hit.frame, 31);
    CHECK(g_B15J.hit.invincible);
    CHECK_EQ_INT(g_B15J.pattern_state, 51);
    CHECK_EQ_INT(g_B15J.patterns_done, 9);
    CHECK_EQ_INT(g_B15J.spiral_angle, 255);
    CHECK(g_Pellets.interlace);
    CHECK_EQ_INT(arena.used, used);
    Arena_Free(&arena);
}

TEST(asset_validation_checks_ripples_and_live_banks)
{
    ARENA arena;
    Arena_Init(&arena, "b15j", 2 * 1024 * 1024);
    FIXTURE f;
    M_Init(&f, &arena);
    CHECK(Result_Ignore(B15J_ValidateAssets(&f.assets)));
    f.assets.ripples.count = 2;
    CHECK(!Result_Ignore(B15J_ValidateAssets(&f.assets)));
    f.assets.ripples.count = 1;
    f.assets.tears.coverage = nullptr;
    CHECK(!Result_Ignore(B15J_ValidateAssets(&f.assets)));
    CHECK(!Result_Ignore(B15J_ValidateAssets(nullptr)));
    Arena_Free(&arena);
}

TEST(loader_failure_clears_output)
{
    ARENA arena;
    Arena_Init(&arena, "b15j", 2 * 1024 * 1024);
    VFS *const vfs = Vfs_Create(Arena_Allocator(&arena));
    B15J_ASSETS out = { .souls = { .count = 3 } };
    CHECK(!Result_Ignore(B15J_LoadAssets(vfs, Arena_Allocator(&arena), &out)));
    Vfs_Destroy(vfs);
    CHECK_EQ_INT(out.souls.count, 0);
    CHECK(out.souls.pixels == nullptr);
    Arena_Free(&arena);
}

static void M_Enter(FIXTURE *const f)
{
    for (int32_t i = 0; i < 1000; i++) {
        if (B15J_UpdateAndRender(&f->assets, &f->miko, &f->stage)) {
            return;
        }
        const B15J frozen = g_B15J;
        CHECK(!B15J_UpdateAndRender(&f->assets, &f->miko, &f->stage));
        CHECK(memcmp(&g_B15J, &frozen, sizeof(g_B15J)) == 0);
        Frame_VSync();
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
            B15J_Init();
            B15J_Setup(&f.assets, &f.miko);
            Palette_Set(&g_Palette, &f.assets.palette);
            Video_SelectDrawingPage(page);
            for (int32_t x = 0; x < 640; x++) {
                for (int32_t y = 0; y < 400; y++) {
                    g_Video.pages[1][y * 640 + x] = (uint8_t)((x + y) % 16);
                }
            }
            const PALETTE saved = g_Palette.saved;
            const size_t used = arena.used;
            Frame_Reset();
            g_VSync.count1 = count == 0 ? 0 : 17;
            g_VSync.count2 = 21;
            M_Enter(&f);
            CHECK_EQ_INT(g_B15J.phase, B15J_PHASE_SPIRAL);
            CHECK_EQ_INT(g_B15J.frame, 0);
            CHECK_EQ_INT(g_B15J.hp, 14);
            CHECK_EQ_INT(g_B15J.hit.frame, 0);
            CHECK(!g_B15J.hit.invincible);
            CHECK_EQ_INT(g_VSync.edge, count == 0 ? 752 : 751);
            CHECK_EQ_INT(g_VSync.count2, 21 + (int32_t)g_VSync.edge);
            CHECK_EQ_INT(g_Video.drawing_page, 0);
            CHECK(memcmp(&g_Palette.saved, &saved, sizeof(saved)) == 0);
            CHECK(memcmp(&g_StagePalette, &saved, sizeof(saved)) == 0);
            CHECK(memcmp(&g_B15J.boss_palette, &saved, sizeof(saved)) == 0);
            CHECK_EQ_INT(g_B15J.souls[0].left, 0);
            CHECK_EQ_INT(g_B15J.souls[1].top, 0);
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
        g_Session.settings.difficulty = rank;
        g_Session.settings.difficulty = rank;
        B15J_Init();
        B15J_Setup(&f.assets, &f.miko);
        Frame_Reset();
        M_Enter(&f);
        const size_t used = arena.used;
        uint32_t phases = 0, patterns = 0;
        bool saw_tear = false, saw_ripple = false;
        for (int32_t frame = 0; frame < 12000; frame++) {
            phases |= 1u << g_B15J.phase;
            if (g_B15J.phase == B15J_PHASE_PATTERNS) {
                patterns |= 1u << g_B15J.subphase;
            }
            g_Session.frame_rand++;
            CHECK(B15J_UpdateAndRender(&f.assets, &f.miko, &f.stage));
            for (int32_t i = 0; i < 10; i++) {
                saw_tear |= g_B15J.tear_frame[i] != 0;
                saw_ripple |= g_B15J.tear_frame[i] > 1;
            }
            ShootoutLasers_UpdateAll(&(SHOOTOUT_LASERS_ASSETS) {});
            Frame_VSync();
        }
        CHECK_EQ_INT(
            phases,
            (1u << B15J_PHASE_SPIRAL) | (1u << B15J_PHASE_RECOLOR)
                | (1u << B15J_PHASE_SOULS) | (1u << B15J_PHASE_TEARS)
                | (1u << B15J_PHASE_PATTERNS));
        CHECK_EQ_INT(patterns, 15);
        CHECK(saw_tear);
        CHECK(saw_ripple);
        CHECK_EQ_INT(g_B15J.hp, 14);
        CHECK(g_Rng != 0xb15a0000);
        CHECK_EQ_INT(arena.used, used);
        CHECK(!g_Player.is_hit);
        Arena_Free(&arena);
    }
}

TEST(current_orb_hitbox_and_hp_gates_reach_real_defeat)
{
    ARENA arena;
    Arena_Init(&arena, "b15j", 2 * 1024 * 1024);
    FIXTURE f;
    M_Battle(&f, &arena);
    B15J_Init();
    B15J_Setup(&f.assets, &f.miko);
    Frame_Reset();
    M_Enter(&f);
    const size_t used = arena.used;
    g_Orb.left = 0;
    g_Orb.top = 64;
    g_Orb.previous_left = 300;
    g_Orb.previous_top = 100;
    CHECK(B15J_UpdateAndRender(&f.assets, &f.miko, &f.stage));
    CHECK_EQ_INT(g_B15J.hp, 14);
    g_Orb.left = 300;
    g_Orb.top = 132;
    CHECK(B15J_UpdateAndRender(&f.assets, &f.miko, &f.stage));
    CHECK_EQ_INT(g_B15J.hp, 14);
    g_Orb.top = -100;
    g_Orb.previous_left = 0;
    CHECK(B15J_UpdateAndRender(&f.assets, &f.miko, &f.stage));
    CHECK_EQ_INT(g_B15J.hp, 13);
    CHECK_EQ_INT(g_Stage.score, 7000);
    g_Orb.top = 64;
    g_Pellets.slots[0].cloud_frame = 12;
    m_Lasers.slots[4].alive = true;
    m_Lasers.slots[4].origin_left = 100 * 256;
    for (int32_t frame = 0; frame < 5000 && !g_B15J.finished; frame++) {
        const bool attack =
            (g_B15J.phase == B15J_PHASE_SPIRAL && g_B15J.hp > 10)
            || (g_B15J.phase == B15J_PHASE_TEARS && g_B15J.hp > 6)
            || (g_B15J.phase == B15J_PHASE_PATTERNS && g_B15J.hp > 0);
        g_Orb.left = attack ? 300 : 0;
        const bool completed =
            B15J_UpdateAndRender(&f.assets, &f.miko, &f.stage);
        if (!completed) {
            const B15J frozen = g_B15J;
            CHECK(!B15J_UpdateAndRender(&f.assets, &f.miko, &f.stage));
            CHECK(memcmp(&g_B15J, &frozen, sizeof(g_B15J)) == 0);
        }
        Frame_VSync();
    }
    CHECK(g_B15J.finished);
    CHECK_EQ_INT(g_B15J.hp, 0);
    CHECK_EQ_INT(g_Stage.score, 98000);
    CHECK(g_Stage.cleared);
    CHECK(g_Player.is_hit);
    CHECK(m_Scene.active == &f.assets.next_scene);
    CHECK_EQ_INT(m_Scene.active->id, 6);
    CHECK_EQ_INT(g_Pellets.slots[0].cloud_frame, 12);
    CHECK(m_Lasers.slots[4].alive);
    const B15J finished = g_B15J;
    CHECK(B15J_UpdateAndRender(&f.assets, &f.miko, &f.stage));
    CHECK(memcmp(&g_B15J, &finished, sizeof(g_B15J)) == 0);
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
        g_Session.settings.difficulty = rank;
        g_Session.settings.difficulty = rank;
        B15J_Init();
        B15J_Setup(&f.assets, &f.miko);
        g_B15J.phase = B15J_PHASE_PATTERNS;
        g_B15J.subphase = 1;
        g_B15J.frame = 249;
        m_Lasers.slots[0] =
            (SHOOTOUT_LASER) { .alive = true, .age = 77, .origin_left = 3210 };
        uint32_t expected = g_Rng;
        MasterRng_Next(&expected);
        Frame_Reset();
        CHECK(B15J_UpdateAndRender(&f.assets, &f.miko, &f.stage));
        CHECK_EQ_INT(g_B15J.pattern_state, 52 + rank * 4);
        CHECK_EQ_INT(g_Rng, expected);
        CHECK_EQ_INT(m_Lasers.slots[0].age, 77);
        CHECK_EQ_INT(m_Lasers.slots[0].origin_left, 3210);
        g_B15J.subphase = 2;
        g_B15J.frame = Math16_Wrap(g_B15J.pattern_state * 2 - 1);
        CHECK(B15J_UpdateAndRender(&f.assets, &f.miko, &f.stage));
        int32_t alive = 0;
        for (int32_t i = 0; i < PELLETS_COUNT; i++) {
            alive += g_Pellets.slots[i].moving;
        }
        CHECK_EQ_INT(alive, 12);
        CHECK_EQ_INT(g_B15J.pattern_state, 52 + rank * 4);
        g_B15J.subphase = 3;
        g_B15J.frame = 99;
        MasterRng_Next(&expected);
        CHECK(B15J_UpdateAndRender(&f.assets, &f.miko, &f.stage));
        CHECK_EQ_INT(g_Rng, expected);
        CHECK_EQ_INT(g_B15J.pattern_state, 60 + rank * 4);
        CHECK_EQ_INT(
            g_B15J.random_range,
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
    B15J_Init();
    B15J_Setup(&f.assets, &f.miko);
    g_B15J.phase = B15J_PHASE_PATTERNS;
    g_B15J.subphase = 1;
    g_B15J.patterns_done = 30;
    const HUD_HP health = m_Health;
    Frame_Reset();
    CHECK(B15J_UpdateAndRender(&f.assets, &f.miko, &f.stage));
    CHECK_EQ_INT(g_B15J.hp, 1);
    CHECK(memcmp(&m_Health, &health, sizeof(health)) == 0);
    CHECK_EQ_INT(g_Stage.score, 0);
    g_TestDamage = true;
    CHECK(!B15J_UpdateAndRender(&f.assets, &f.miko, &f.stage));
    CHECK_EQ_INT(g_B15J.hp, 0);
    CHECK(!g_TestDamage);
    CHECK(g_B15J.defeating);
    CHECK_EQ_INT(g_Stage.score, 0);
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
        B15J_Init();
        B15J_Setup(&f.assets, &f.miko);
        g_B15J.phase = B15J_PHASE_PATTERNS;
        g_B15J.subphase = 1;
        g_B15J.tear_frame[0] = 1;
        g_B15J.tears[0].left = 37;
        g_B15J.tears[0].top = 392;
        g_Player.invincible = false;
        g_Player.left = positions[i];
        Frame_Reset();
        CHECK(B15J_UpdateAndRender(&f.assets, &f.miko, &f.stage));
        CHECK_EQ_INT(g_B15J.tear_frame[0], 2);
        CHECK_EQ_INT(g_Player.is_hit, i == 1 || i == 2);
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
        B15J_Init();
        B15J_Setup(&f.assets, &f.miko);
        g_B15J.phase = B15J_PHASE_SOULS;
        g_B15J.subphase = 1;
        g_B15J.frame = 79;
        g_B15J.souls[0].left = 100;
        g_B15J.souls[1].left = 500;
        g_B15J.souls[0].top = g_B15J.souls[1].top = 100;
        g_Pellets.spawn_with_cloud = cloud != 0;
        const size_t used = arena.used;
        Frame_Reset();
        CHECK(B15J_UpdateAndRender(&f.assets, &f.miko, &f.stage));
        int32_t count = 0;
        for (int32_t i = 0; i < PELLETS_COUNT; i++) {
            count += cloud != 0 ? g_Pellets.slots[i].cloud_frame == 1
                                : g_Pellets.slots[i].moving;
        }
        CHECK_EQ_INT(count, 12);
        CHECK_EQ_INT(g_Pellets.spawn_with_cloud, cloud != 0);
        CHECK(g_Pellets.interlace);
        CHECK_EQ_INT(arena.used, used);
        Arena_Free(&arena);
    }
}
