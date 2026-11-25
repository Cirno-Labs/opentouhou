#include "../src/th01/shootout_lasers.c"
#include "../src/th01/scene.c"
#include "../src/th01/missiles.c"
#include "th01/boss_b15m.h"
#include "th01/sound.h"
#include "th01/shots.h"
#include "th01/player.h"
#include "th01/pellets.h"
#include "th01/orb.h"
#include "th01/rng.h"
#include "th01/session.h"
#include "pc98/zun/master_rng.h"

#include "core/base/arena.h"

#include "th01_world.h"
#include "test.h"

typedef struct {
    B15M_ASSETS assets;
    GAME_TEXT_FONT font;
    PTN stage, miko;
} FIXTURE;

static void M_Init(FIXTURE *const f, ARENA *const arena)
{
    *f = (FIXTURE) {};
    Th01Test_ResetWorld();
    const ALLOCATOR *const a = Arena_Allocator(arena);
    Video_Init(a);
    f->assets.girl = (BOS) { 128, 96, 6, Arena_AllocZ(arena, 128 * 96 * 6),
                             Arena_AllocZ(arena, 128 * 96 * 6) };
    f->assets.attack = (BOS) { 128, 96, 3, Arena_AllocZ(arena, 128 * 96 * 3),
                               Arena_AllocZ(arena, 128 * 96 * 3) };
    f->assets.bat = (BOS) { 48, 32, 3, Arena_AllocZ(arena, 48 * 32 * 3),
                            Arena_AllocZ(arena, 48 * 32 * 3) };
    f->assets.spheres = (GRC) { 32, 32, 6, Arena_AllocZ(arena, 32 * 32 * 6) };
    f->assets.star = Arena_AllocZ(arena, 8);
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
        Result_Ignore(Scene_LoadReader(&reader, 5, a, &f->assets.next_scene)));
    Reader_Close(&reader);
    g_Session.frame_rand = 0x12345678;
}

TEST(setup_reseeds_without_resetting_actor_or_missile_history)
{
    ARENA arena;
    Arena_Init(&arena, "b15m", 2 * 1024 * 1024);
    FIXTURE f;
    M_Init(&f, &arena);
    B15M_Init();
    memset(g_B15M.background, 0, 128 * 96);
    g_B15M.hit = (BOSS_HIT) { 27, true };
    g_B15M.pattern_state = 37;
    g_B15M.girl.image = 4;
    g_B15M.attack.orb_hitbox = (BOSS_BOUNDS) { 1, 2, 3, 4 };
    g_B15M.bat.lock_frame = 7;
    g_B15M.girl.previous_left = 199;
    g_B15M.girl.orb_inactive = true;
    g_B15M.missile_rifts.x[0] = 88;
    m_Missiles.slots[0].flag = MISSILE_MOVING;
    m_Missiles.slots[0].left = 300;
    const MISSILES missiles = m_Missiles;
    const PALETTE saved = g_Palette.saved;
    const size_t used = arena.used;
    B15M_Setup(&f.assets, &f.miko);
    CHECK_EQ_INT(arena.used, used);
    CHECK_EQ_INT(g_B15M.hp, 14);
    CHECK_EQ_INT(g_B15M.phase, B15M_PHASE_ENTRANCE);
    CHECK_EQ_INT(g_B15M.frame, 0);
    CHECK_EQ_INT(g_HudHp.first_white, 10);
    CHECK_EQ_INT(g_HudHp.first_redwhite, 6);
    CHECK_EQ_INT(g_Rng, 0x12345678);
    CHECK(g_Pellets.interlace);
    CHECK(memcmp(&m_Missiles, &missiles, sizeof(missiles)) == 0);
    CHECK(memcmp(&g_Palette.saved, &saved, sizeof(saved)) == 0);
    CHECK_EQ_INT(g_B15M.girl.left, 256);
    CHECK_EQ_INT(g_B15M.girl.top, 96);
    CHECK_EQ_INT(g_B15M.attack.left, 256);
    CHECK_EQ_INT(g_B15M.bat.top, 96);
    CHECK_EQ_INT(g_B15M.girl.image, 4);
    CHECK_EQ_INT(g_B15M.girl.previous_left, 199);
    CHECK(g_B15M.girl.orb_inactive);
    CHECK_EQ_INT(g_B15M.bat.lock_frame, 7);
    CHECK_EQ_INT(g_B15M.hit.frame, 27);
    CHECK(g_B15M.hit.invincible);
    CHECK_EQ_INT(g_B15M.pattern_state, 37);
    CHECK_EQ_INT(g_B15M.missile_rifts.x[0], 88);
    CHECK_EQ_INT(g_B15M.girl.orb_hitbox.left, 32);
    CHECK_EQ_INT(g_B15M.girl.orb_hitbox.right, 96);
    CHECK_EQ_INT(g_B15M.girl.orb_hitbox.top, 12);
    CHECK_EQ_INT(g_B15M.girl.orb_hitbox.bottom, 64);
    CHECK_EQ_INT(g_B15M.bat.orb_hitbox.left, 8);
    CHECK_EQ_INT(g_B15M.bat.orb_hitbox.right, 40);
    CHECK_EQ_INT(g_B15M.bat.orb_hitbox.top, 8);
    CHECK_EQ_INT(g_B15M.bat.orb_hitbox.bottom, 24);
    CHECK_EQ_INT(g_B15M.attack.orb_hitbox.left, 1);
    CHECK_EQ_INT(g_B15M.attack.orb_hitbox.bottom, 4);
    CHECK(g_BossSprites.entities[0] == &f.assets.girl);
    CHECK(g_BossSprites.entities[1] == &f.assets.attack);
    CHECK(g_BossSprites.entities[2] == &f.assets.bat);
    const PALETTE black = {};
    CHECK(memcmp(&g_B15M.post_defeat, &black, sizeof(black)) == 0);
    Arena_Free(&arena);
}

TEST(asset_validation_requires_all_live_banks_and_next_scene)
{
    ARENA arena;
    Arena_Init(&arena, "b15m", 2 * 1024 * 1024);
    FIXTURE f;
    M_Init(&f, &arena);
    CHECK(Result_Ignore(B15M_ValidateAssets(&f.assets)));
    B15M_ASSETS bad = f.assets;
    bad.girl.count = 5;
    CHECK(!Result_Ignore(B15M_ValidateAssets(&bad)));
    bad = f.assets;
    bad.star = nullptr;
    CHECK(!Result_Ignore(B15M_ValidateAssets(&bad)));
    bad = f.assets;
    bad.spheres.count = 5;
    CHECK(!Result_Ignore(B15M_ValidateAssets(&bad)));
    bad = f.assets;
    bad.next_scene.id = 6;
    CHECK(!Result_Ignore(B15M_ValidateAssets(&bad)));
    Arena_Free(&arena);
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

static void M_Run(FIXTURE *const f, uint64_t *const tick)
{
    int32_t polls = 0;
    while (!B15M_UpdateAndRender(&f->assets, &f->miko, *tick)) {
        CHECK(polls++ < 2000);
        if (polls >= 2000) {
            break;
        }
        (*tick)++;
        Frame_VSync();
    }
}

TEST(entrance_waits_preserve_prefix_once_and_source_counter_history)
{
    const int16_t initial[] = { 0, 1, 12 };
    const int16_t expected[] = { 799, 798, 789 };
    for (int32_t i = 0; i < 3; i++) {
        ARENA arena;
        Arena_Init(&arena, "b15m", 4 * 1024 * 1024);
        FIXTURE f;
        M_Battle(&f, &arena);
        B15M_Init();
        memset(g_B15M.background, 0, 128 * 96);
        B15M_Setup(&f.assets, &f.miko);
        m_Missiles.slots[0] = (MISSILE) { .left = 200 * 16,
                                          .top = 200 * 16,
                                          .previous_left = -1,
                                          .velocity = { 16, 0 },
                                          .flag = MISSILE_MOVING };
        Frame_Reset();
        g_VSync.count1 = initial[i];
        g_VSync.count2 = 9;
        uint64_t tick = 0;
        CHECK(!B15M_UpdateAndRender(&f.assets, &f.miko, tick));
        CHECK(g_B15M.entrance_active);
        CHECK_EQ_INT(m_Missiles.slots[0].left, 201 * 16);
        const B15M frozen = g_B15M;
        const uint32_t rng = g_Rng;
        CHECK(!B15M_UpdateAndRender(&f.assets, &f.miko, tick));
        CHECK(memcmp(&g_B15M, &frozen, sizeof(g_B15M)) == 0);
        CHECK_EQ_INT(g_Rng, rng);
        const size_t used = arena.used;
        M_Run(&f, &tick);
        CHECK_EQ_INT(tick, expected[i]);
        CHECK_EQ_INT(g_VSync.count2, 9 + expected[i]);
        CHECK_EQ_INT(g_VSync.count1, 0);
        CHECK_EQ_INT(g_B15M.phase, B15M_PHASE_PATTERNS_1);
        CHECK_EQ_INT(g_B15M.frame, 0);
        CHECK_EQ_INT(g_B15M.hit.frame, 0);
        CHECK_EQ_INT(g_B15M.pattern, 1);
        CHECK(!g_B15M.entrance_active);
        CHECK(!g_B15M.girl.orb_inactive);
        CHECK_EQ_INT(m_Missiles.slots[0].left, 201 * 16);
        CHECK_EQ_INT(arena.used, used);
        Arena_Free(&arena);
    }
}

TEST(raw_scroll_wait_freezes_then_runs_only_the_remaining_call)
{
    ARENA arena;
    Arena_Init(&arena, "b15m", 4 * 1024 * 1024);
    FIXTURE f;
    M_Battle(&f, &arena);
    B15M_Init();
    memset(g_B15M.background, 0, 128 * 96);
    B15M_Setup(&f.assets, &f.miko);
    g_B15M.phase = B15M_PHASE_TRANSFORM;
    g_B15M.frame = 19;
    g_B15M.teleport_done = true;
    g_B15M.hit.frame = 17;
    Frame_Reset();
    g_VSync.count1 = 12;
    g_VSync.count2 = 25;
    CHECK(!B15M_UpdateAndRender(&f.assets, &f.miko, 0));
    CHECK_EQ_INT(g_B15M.pending, B15M_PENDING_SCROLL);
    CHECK_EQ_INT(g_B15M.frame, 20);
    const B15M frozen = g_B15M;
    const uint32_t rng = g_Rng;
    CHECK(!B15M_UpdateAndRender(&f.assets, &f.miko, 0));
    CHECK(memcmp(&g_B15M, &frozen, sizeof(g_B15M)) == 0);
    CHECK_EQ_INT(g_Rng, rng);
    Frame_VSync();
    CHECK(B15M_UpdateAndRender(&f.assets, &f.miko, 1));
    CHECK_EQ_INT(g_B15M.phase, B15M_PHASE_TRANSFORM);
    CHECK_EQ_INT(g_B15M.pattern, 0);
    CHECK_EQ_INT(g_B15M.frame, 20);
    CHECK_EQ_INT(g_B15M.hit.frame, 17);
    CHECK_EQ_INT(g_VSync.count1, 13);
    CHECK_EQ_INT(g_VSync.count2, 26);
    CHECK_EQ_INT(g_Video.scroll_top, 0);
    Arena_Free(&arena);
}

TEST(safety_circle_delays_hit_and_preserves_later_source_effects)
{
    ARENA arena;
    Arena_Init(&arena, "b15m", 4 * 1024 * 1024);
    FIXTURE f;
    M_Battle(&f, &arena);
    B15M_Init();
    memset(g_B15M.background, 0, 128 * 96);
    B15M_Setup(&f.assets, &f.miko);
    g_B15M.phase = B15M_PHASE_FORMS;
    g_B15M.form = B15M_FORM_GIRL;
    g_B15M.frame = 140;
    g_B15M.girl_pattern = 2;
    g_B15M.girl_subphase = 1;
    g_B15M.pattern_state = 2;
    g_B15M.safety_circle = (B15M_CIRCLE) { 64, 1 };
    g_B15M.safety_left = 200;
    g_Player.left = 400;
    g_Player.invincible = false;
    Frame_Reset();
    CHECK(!B15M_UpdateAndRender(&f.assets, &f.miko, 100));
    CHECK_EQ_INT(g_B15M.pending, B15M_PENDING_SAFETY);
    CHECK_EQ_INT(g_B15M.safety_circle.frame, 2);
    CHECK(!g_Player.is_hit);
    const B15M frozen = g_B15M;
    const uint32_t rng = g_Rng;
    CHECK(!B15M_UpdateAndRender(&f.assets, &f.miko, 105));
    CHECK(memcmp(&g_B15M, &frozen, sizeof(g_B15M)) == 0);
    CHECK_EQ_INT(g_Rng, rng);
    CHECK(!g_Player.is_hit);
    CHECK(B15M_UpdateAndRender(&f.assets, &f.miko, 106));
    CHECK(g_Player.is_hit);
    CHECK_EQ_INT(g_B15M.safety_circle.frame, 200);
    CHECK_EQ_INT(g_B15M.frame, 141);
    CHECK(g_Pellets.slots[0].moving);
    CHECK_EQ_INT(g_B15M.girl_subphase, 1);
    CHECK_EQ_INT(g_B15M.phase5_subphase, 0);
    Arena_Free(&arena);
}

TEST(real_orb_battle_visits_every_form_and_pattern_then_defeats)
{
    for (int32_t rank = 0; rank < 4; rank++) {
        ARENA arena;
        Arena_Init(&arena, "b15m", 4 * 1024 * 1024);
        FIXTURE f;
        M_Battle(&f, &arena);
        g_Session.settings.difficulty = rank;
        g_Session.settings.difficulty = rank;
        g_Session.frame_rand = 0xb15f0000u + (uint32_t)rank;
        B15M_Init();
        memset(g_B15M.background, 0, 128 * 96);
        B15M_Setup(&f.assets, &f.miko);
        Frame_Reset();
        uint64_t tick = 0;
        M_Run(&f, &tick);
        const size_t used = arena.used;
        uint32_t phase1 = 0, phase3 = 0, girl = 0, bat = 0;
        int32_t p1ticks = 0, p3ticks = 0;
        for (int32_t step = 1; step < 40000 && !g_B15M.finished; step++) {
            g_Session.frame_rand = (uint32_t)step;
            if (g_B15M.phase == B15M_PHASE_PATTERNS_1) {
                p1ticks++;
                if (g_B15M.pattern > 0 && g_B15M.pattern <= 3) {
                    phase1 |= 1u << g_B15M.pattern;
                }
            }
            if (g_B15M.phase == B15M_PHASE_PATTERNS_2) {
                p3ticks++;
                if (g_B15M.pattern > 0 && g_B15M.pattern <= 3
                    && g_B15M.phase3_pattern != 0) {
                    phase3 |= 1u << g_B15M.pattern;
                }
            }
            if (g_B15M.phase == B15M_PHASE_FORMS) {
                if (g_B15M.form == B15M_FORM_GIRL && g_B15M.girl_pattern > 0
                    && g_B15M.girl_pattern <= 3) {
                    girl |= 1u << g_B15M.girl_pattern;
                }
                if (g_B15M.form == B15M_FORM_BAT && g_B15M.bat_pattern > 0
                    && g_B15M.bat_pattern <= 4) {
                    bat |= 1u << g_B15M.bat_pattern;
                }
            }
            int16_t target = g_B15M.hp;
            if (g_B15M.phase == B15M_PHASE_PATTERNS_1 && phase1 == 14
                && p1ticks > 2500) {
                target = 10;
            } else if (
                g_B15M.phase == B15M_PHASE_PATTERNS_2 && phase3 == 14
                && p3ticks > 2000) {
                target = 6;
            } else if (
                g_B15M.phase == B15M_PHASE_FORMS && girl == 14 && bat == 30) {
                target = 0;
            }
            const BOSS_ENTITY *const e =
                g_B15M.form == B15M_FORM_GIRL ? &g_B15M.girl : &g_B15M.bat;
            const bool hit = g_B15M.hp > target && step % 50 == 20;
            g_Orb.previous_left = hit ? e->left + (g_B15M.form ? 16 : 48) : 0;
            g_Orb.previous_top = hit ? e->top + 24 : 399;
            M_Run(&f, &tick);
            Frame_VSync();
            tick++;
        }
        CHECK_EQ_INT(phase1, 14);
        CHECK_EQ_INT(phase3, 14);
        CHECK_EQ_INT(girl, 14);
        CHECK_EQ_INT(bat, 30);
        CHECK(g_B15M.finished);
        CHECK_EQ_INT(g_B15M.hp, 0);
        CHECK_EQ_INT(g_Stage.score, 98000);
        CHECK(g_Stage.cleared);
        CHECK(g_Player.is_hit);
        CHECK(m_Scene.active == &f.assets.next_scene);
        CHECK_EQ_INT(m_Scene.active->id, 5);
        CHECK_EQ_INT(arena.used, used);
        const B15M done = g_B15M;
        CHECK(B15M_UpdateAndRender(&f.assets, &f.miko, tick));
        CHECK(memcmp(&g_B15M, &done, sizeof(g_B15M)) == 0);
        Arena_Free(&arena);
    }
}

TEST(safety_hit_tail_stacks_real_defeat_without_repeating_the_prefix)
{
    ARENA arena;
    Arena_Init(&arena, "b15m", 4 * 1024 * 1024);
    FIXTURE f;
    M_Battle(&f, &arena);
    B15M_Init();
    memset(g_B15M.background, 0, 128 * 96);
    B15M_Setup(&f.assets, &f.miko);
    g_B15M.phase = B15M_PHASE_FORMS;
    g_B15M.hp = 1;
    g_B15M.frame = 140;
    g_B15M.form = B15M_FORM_GIRL;
    g_B15M.girl_pattern = 2;
    g_B15M.girl_subphase = 1;
    g_B15M.pattern_state = 2;
    g_B15M.safety_circle = (B15M_CIRCLE) { 64, 1 };
    g_B15M.safety_left = 200;
    g_Player.left = 400;
    g_Player.invincible = false;
    g_Orb.previous_left = g_B15M.girl.left + 48;
    g_Orb.previous_top = g_B15M.girl.top + 24;
    m_Missiles.slots[0] = (MISSILE) { .left = 200 * 16,
                                      .top = 200 * 16,
                                      .previous_left = -1,
                                      .velocity = { 16, 0 },
                                      .flag = MISSILE_MOVING };
    g_Pellets.slots[1].cloud_frame = 10;
    for (int32_t i = 0; i < 10; i++) {
        m_Lasers.slots[i].alive = true;
    }
    Frame_Reset();
    const uint32_t rng = g_Rng;
    const size_t used = arena.used;
    CHECK(!B15M_UpdateAndRender(&f.assets, &f.miko, 100));
    CHECK_EQ_INT(g_B15M.pending, B15M_PENDING_SAFETY);
    CHECK(!g_Player.is_hit);
    CHECK(!g_Stage.cleared);
    CHECK_EQ_INT(m_Missiles.slots[0].left, 201 * 16);
    CHECK(!B15M_UpdateAndRender(&f.assets, &f.miko, 105));
    CHECK(!g_Player.is_hit);
    CHECK(!B15M_UpdateAndRender(&f.assets, &f.miko, 106));
    CHECK(g_B15M.defeating);
    CHECK_EQ_INT(g_B15M.hp, 0);
    CHECK(g_Player.is_hit);
    CHECK(!g_Stage.cleared);
    CHECK(m_Scene.active == nullptr);
    CHECK_EQ_INT(g_Stage.score, 7000);
    CHECK_EQ_INT(g_Rng, rng);
    CHECK_EQ_INT(m_Missiles.slots[0].flag, MISSILE_FREE);
    CHECK_EQ_INT(m_Missiles.slots[0].left, 201 * 16);
    CHECK_EQ_INT(g_Pellets.slots[1].cloud_frame, 10);
    for (int32_t i = 0; i < 10; i++) {
        CHECK(!m_Lasers.slots[i].alive);
    }
    const B15M frozen = g_B15M;
    CHECK(!B15M_UpdateAndRender(&f.assets, &f.miko, 106));
    CHECK(memcmp(&g_B15M, &frozen, sizeof(g_B15M)) == 0);
    uint64_t tick = 106;
    M_Run(&f, &tick);
    CHECK(g_B15M.finished);
    CHECK(g_Stage.cleared);
    CHECK(m_Scene.active == &f.assets.next_scene);
    CHECK_EQ_INT(m_Missiles.slots[0].left, 201 * 16);
    CHECK_EQ_INT(g_Rng, rng);
    CHECK_EQ_INT(arena.used, used);
    Arena_Free(&arena);
}

TEST(phase_hp_gate_waits_for_teleport_and_preserves_unused_rng_draws)
{
    ARENA arena;
    Arena_Init(&arena, "b15m", 4 * 1024 * 1024);
    FIXTURE f;
    M_Battle(&f, &arena);
    B15M_Init();
    memset(g_B15M.background, 0, 128 * 96);
    B15M_Setup(&f.assets, &f.miko);
    g_B15M.phase = B15M_PHASE_PATTERNS_1;
    g_B15M.hp = 10;
    g_B15M.pattern = 0;
    g_B15M.frame = 79;
    g_B15M.hp_bar_filled = true;
    uint32_t expected = g_Rng;
    for (int32_t i = 0; i < 12; i++) {
        MasterRng_Next(&expected);
    }
    Frame_Reset();
    CHECK(B15M_UpdateAndRender(&f.assets, &f.miko, 0));
    CHECK_EQ_INT(g_B15M.phase, B15M_PHASE_PATTERNS_1);
    CHECK_EQ_INT(g_B15M.frame, 80);
    CHECK(!g_B15M.teleport_done);
    CHECK_EQ_INT(g_Rng, expected);
    MasterRng_Next(&expected);
    MasterRng_Next(&expected);
    CHECK(B15M_UpdateAndRender(&f.assets, &f.miko, 1));
    CHECK_EQ_INT(g_B15M.phase, B15M_PHASE_CENTER);
    CHECK_EQ_INT(g_B15M.frame, 0);
    CHECK_EQ_INT(g_Rng, expected);
    Arena_Free(&arena);
}

TEST(circle_groups_keep_the_callers_cloud_policy)
{
    for (int32_t cloud = 0; cloud < 2; cloud++) {
        ARENA arena;
        Arena_Init(&arena, "b15m", 4 * 1024 * 1024);
        FIXTURE f;
        M_Battle(&f, &arena);
        B15M_Init();
        memset(g_B15M.background, 0, 128 * 96);
        B15M_Setup(&f.assets, &f.miko);
        g_B15M.phase = B15M_PHASE_PATTERNS_1;
        g_B15M.pattern = 3;
        g_B15M.frame = 200;
        g_B15M.circle.frame = 40;
        g_B15M.circle.angle = 64;
        g_B15M.pattern_state = PELLET_GROUP_SINGLE_AIMED;
        g_B15M.hp_bar_filled = true;
        g_Pellets.spawn_with_cloud = cloud != 0;
        Frame_Reset();
        const size_t used = arena.used;
        CHECK(B15M_UpdateAndRender(&f.assets, &f.miko, 0));
        int32_t count = 0;
        for (int32_t i = 0; i < PELLETS_COUNT; i++) {
            count += cloud ? g_Pellets.slots[i].cloud_frame != 0
                           : g_Pellets.slots[i].moving;
        }
        CHECK_EQ_INT(count, 23);
        CHECK_EQ_INT(g_Pellets.spawn_with_cloud, cloud != 0);
        CHECK(g_Pellets.interlace);
        CHECK_EQ_INT(arena.used, used);
        Arena_Free(&arena);
    }
}

TEST(full_missile_pool_still_consumes_pattern_randomness)
{
    ARENA arena;
    Arena_Init(&arena, "b15m", 4 * 1024 * 1024);
    FIXTURE f;
    M_Battle(&f, &arena);
    B15M_Init();
    memset(g_B15M.background, 0, 128 * 96);
    B15M_Setup(&f.assets, &f.miko);
    g_B15M.phase = B15M_PHASE_PATTERNS_1;
    g_B15M.pattern = 2;
    g_B15M.frame = 59;
    g_B15M.hp_bar_filled = true;
    g_B15M.pattern_state = 15;
    for (int32_t i = 0; i < MISSILES_COUNT; i++) {
        m_Missiles.slots[i] = (MISSILE) { .left = 200 * 16,
                                          .top = 200 * 16,
                                          .previous_left = -1,
                                          .flag = MISSILE_MOVING };
    }
    Frame_Reset();
    CHECK(B15M_UpdateAndRender(&f.assets, &f.miko, 0));
    g_B15M.frame = 62;
    uint32_t expected = g_Rng;
    MasterRng_Next(&expected);
    MasterRng_Next(&expected);
    CHECK(B15M_UpdateAndRender(&f.assets, &f.miko, 1));
    CHECK_EQ_INT(g_Rng, expected);
    for (int32_t i = 0; i < MISSILES_COUNT; i++) {
        CHECK_EQ_INT(m_Missiles.slots[i].flag, MISSILE_MOVING);
        CHECK_EQ_INT(m_Missiles.slots[i].left, 200 * 16);
    }
    Arena_Free(&arena);
}
