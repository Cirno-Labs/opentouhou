#include "th01/boss_b15j.h"
#include "pc98/zun/master_rng.h"
#include "pc98/zun/math16.h"

#include "core/base/arena.h"

#include "test.h"

typedef struct {
    PALETTE_STATE palette;
    PALETTE stage_palette;
    B15J_ASSETS assets;
    BOSS_SPRITES bank;
    HUD_HP health;
    PARTICLES particles;
    HUD_CONTEXT display;
    BOSS_HIT_CONTEXT hit_context;
    B15J_CONTEXT context;
    SHOOTOUT_LASERS lasers;
    PELLETS pellets;
    PELLET_SHARED shared;
    HUD hud;
    PLAYER player;
    ORB orb;
    SHOTS shots;
    STAGE_GAMEPLAY gameplay;
    GAME_TEXT_FONT font;
    PTN stage, miko;
    SCENE_STATE scene;
    uint32_t rng, high_score;
    int16_t lives;
    bool test_damage, cleared;
    int32_t effects[17], fades;
} FIXTURE;

static void M_Init(FIXTURE *const f, ARENA *const arena)
{
    *f = (FIXTURE) {};
    const ALLOCATOR *const a = Arena_Allocator(arena);
    Video_Init(a);
    Palette_Init(&f->palette);
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
    f->hit_context = (BOSS_HIT_CONTEXT) { .palette = &f->palette,
                                          .display = &f->display,
                                          .health = &f->health };
    f->context = (B15J_CONTEXT) { .assets = &f->assets,
                                  .sprites = &f->bank,
                                  .hit = &f->hit_context,
                                  .stage_palette = &f->stage_palette,
                                  .particles = &f->particles,
                                  .pellets = &f->pellets };
}

static void M_Effect(void *const user, const int16_t number)
{
    FIXTURE *const f = user;
    CHECK(number >= 1 && number <= 16);
    f->effects[number]++;
}

static void M_Fade(void *const user)
{
    ((FIXTURE *)user)->fades++;
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

static void M_Battle(FIXTURE *const f, ARENA *const arena)
{
    M_Init(f, arena);
    const ALLOCATOR *const a = Arena_Allocator(arena);
    Hud_Init(&f->hud, a);
    HudHp_Init(&f->health, a);
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
    f->lives = 3;
    f->rng = 0xb15a0000;
    f->player.invincible = true;
    f->player.left = 300;
    f->orb.top = 64;
    f->orb.previous_top = 64;
    f->display = (HUD_CONTEXT) { .sprites = &f->stage,
                                 .font = &f->font,
                                 .gameplay = &f->gameplay,
                                 .player = &f->player,
                                 .high_score = &f->high_score,
                                 .remaining_lives = &f->lives };
    f->hit_context.hud = &f->hud;
    f->hit_context.shots = &f->shots;
    f->hit_context.shots_output = (SHOTS_OUTPUT) { M_Shot, f };
    f->hit_context.orb = &f->orb;
    f->hit_context.test_damage = &f->test_damage;
    f->hit_context.effect = M_Effect;
    f->hit_context.user = f;
    f->context.pellets = &f->pellets;
    f->context.pellet_shared = &f->shared;
    f->context.lasers = &f->lasers;
    f->context.rng = &f->rng;
    f->context.miko = &f->miko;
    f->context.scene = &f->scene;
    f->context.stage_cleared = &f->cleared;
    f->context.fade_music = M_Fade;
    CHECK(Result_Ignore(B15J_ValidateContext(&f->context)));
}

TEST(setup_binds_shared_banks_and_keeps_actor_history)
{
    ARENA arena;
    Arena_Init(&arena, "b15j", 2 * 1024 * 1024);
    FIXTURE f;
    M_Init(&f, &arena);
    B15J b = { .hit = { .frame = 31, .invincible = true },
               .pattern_state = 51,
               .patterns_done = 9,
               .spiral_angle = 255 };
    for (int32_t i = 0; i < 2; i++) {
        b.souls[i].image = i + 1;
        b.souls[i].lock_frame = 7 + i;
        b.souls[i].previous_left = 90 + i;
        b.souls[i].orb_inactive = true;
    }
    for (int32_t i = 0; i < 10; i++) {
        b.tears[i].left = 100 + i;
        b.tears[i].image = 2;
        b.tear_frame[i] = 74;
    }
    const PALETTE saved = f.palette.saved;
    const size_t used = arena.used;
    B15J_Setup(&b, &f.context);
    CHECK_EQ_INT(b.hp, 14);
    CHECK_EQ_INT(b.phase, 0);
    CHECK_EQ_INT(b.frame, 0);
    CHECK_EQ_INT(f.health.first_white, 10);
    CHECK_EQ_INT(f.health.first_redwhite, 6);
    CHECK(f.bank.entities[0] == &f.assets.souls);
    CHECK(f.bank.entities[1] == &f.assets.tears);
    for (int32_t i = 0; i < 2; i++) {
        CHECK_EQ_INT(b.souls[i].left, 0);
        CHECK_EQ_INT(b.souls[i].top, 0);
        CHECK_EQ_INT(b.souls[i].image, i + 1);
        CHECK_EQ_INT(b.souls[i].lock_frame, 7 + i);
        CHECK_EQ_INT(b.souls[i].previous_left, 90 + i);
        CHECK(b.souls[i].orb_inactive);
        CHECK_EQ_INT(b.souls[i].move_clamp.left, 32);
        CHECK_EQ_INT(b.souls[i].move_clamp.right, 576);
        CHECK_EQ_INT(b.souls[i].move_clamp.top, 64);
        CHECK_EQ_INT(b.souls[i].move_clamp.bottom, 400);
    }
    for (int32_t i = 0; i < 10; i++) {
        CHECK_EQ_INT(b.tears[i].left, 100 + i);
        CHECK_EQ_INT(b.tears[i].image, 2);
        CHECK_EQ_INT(b.tears[i].sprites.slot, 1);
        CHECK_EQ_INT(b.tears[i].sprites.count, 3);
        CHECK_EQ_INT(b.tear_frame[i], 0);
    }
    CHECK(memcmp(&f.palette.saved, &saved, sizeof(saved)) == 0);
    CHECK(memcmp(&b.boss_palette, &saved, sizeof(saved)) == 0);
    CHECK_EQ_INT(b.post_defeat.colors[15][2], 0);
    CHECK_EQ_INT(b.hit.frame, 31);
    CHECK(b.hit.invincible);
    CHECK_EQ_INT(b.pattern_state, 51);
    CHECK_EQ_INT(b.patterns_done, 9);
    CHECK_EQ_INT(b.spiral_angle, 255);
    CHECK(f.pellets.interlace);
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

static void M_Enter(B15J *const b, FIXTURE *const f)
{
    for (int32_t i = 0; i < 1000; i++) {
        if (B15J_UpdateAndRender(b, &f->context)) {
            return;
        }
        const B15J frozen = *b;
        CHECK(!B15J_UpdateAndRender(b, &f->context));
        CHECK(memcmp(b, &frozen, sizeof(*b)) == 0);
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
            B15J b = {};
            B15J_Setup(&b, &f.context);
            Palette_Set(&f.palette, &f.assets.palette);
            Video_SelectDrawingPage(page);
            for (int32_t x = 0; x < 640; x++) {
                for (int32_t y = 0; y < 400; y++) {
                    g_Video.pages[1][y * 640 + x] = (uint8_t)((x + y) % 16);
                }
            }
            const PALETTE saved = f.palette.saved;
            const size_t used = arena.used;
            Frame_Reset();
            g_VSync.count1 = count == 0 ? 0 : 17;
            g_VSync.count2 = 21;
            M_Enter(&b, &f);
            CHECK_EQ_INT(b.phase, 2);
            CHECK_EQ_INT(b.frame, 0);
            CHECK_EQ_INT(b.hp, 14);
            CHECK_EQ_INT(b.hit.frame, 0);
            CHECK(!b.hit.invincible);
            CHECK_EQ_INT(g_VSync.edge, count == 0 ? 752 : 751);
            CHECK_EQ_INT(g_VSync.count2, 21 + (int32_t)g_VSync.edge);
            CHECK_EQ_INT(g_Video.drawing_page, 0);
            CHECK(memcmp(&f.palette.saved, &saved, sizeof(saved)) == 0);
            CHECK(memcmp(&f.stage_palette, &saved, sizeof(saved)) == 0);
            CHECK(memcmp(&b.boss_palette, &saved, sizeof(saved)) == 0);
            CHECK_EQ_INT(b.souls[0].left, 0);
            CHECK_EQ_INT(b.souls[1].top, 0);
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
        f.display.rank = rank;
        B15J b = {};
        B15J_Setup(&b, &f.context);
        Frame_Reset();
        M_Enter(&b, &f);
        const size_t used = arena.used;
        uint32_t phases = 0, patterns = 0;
        bool saw_tear = false, saw_ripple = false;
        for (int32_t frame = 0; frame < 12000; frame++) {
            phases |= 1u << b.phase;
            if (b.phase == 6) {
                patterns |= 1u << b.subphase;
            }
            f.hit_context.frame_rand++;
            CHECK(B15J_UpdateAndRender(&b, &f.context));
            for (int32_t i = 0; i < 10; i++) {
                saw_tear |= b.tear_frame[i] != 0;
                saw_ripple |= b.tear_frame[i] > 1;
            }
            ShootoutLasers_UpdateAll(
                &f.lasers, &f.player, &(SHOOTOUT_LASERS_ASSETS) {});
            Frame_VSync();
        }
        CHECK_EQ_INT(
            phases, (1u << 2) | (1u << 3) | (1u << 4) | (1u << 5) | (1u << 6));
        CHECK_EQ_INT(patterns, 15);
        CHECK(saw_tear);
        CHECK(saw_ripple);
        CHECK_EQ_INT(b.hp, 14);
        CHECK(f.rng != 0xb15a0000);
        CHECK_EQ_INT(arena.used, used);
        CHECK(!f.player.is_hit);
        CHECK_EQ_INT(f.fades, 0);
        Arena_Free(&arena);
    }
}

TEST(current_orb_hitbox_and_hp_gates_reach_real_defeat)
{
    ARENA arena;
    Arena_Init(&arena, "b15j", 2 * 1024 * 1024);
    FIXTURE f;
    M_Battle(&f, &arena);
    B15J b = {};
    B15J_Setup(&b, &f.context);
    Frame_Reset();
    M_Enter(&b, &f);
    const size_t used = arena.used;
    f.orb.left = 0;
    f.orb.top = 64;
    f.orb.previous_left = 300;
    f.orb.previous_top = 100;
    CHECK(B15J_UpdateAndRender(&b, &f.context));
    CHECK_EQ_INT(b.hp, 14);
    f.orb.left = 300;
    f.orb.top = 132;
    CHECK(B15J_UpdateAndRender(&b, &f.context));
    CHECK_EQ_INT(b.hp, 14);
    f.orb.top = -100;
    f.orb.previous_left = 0;
    CHECK(B15J_UpdateAndRender(&b, &f.context));
    CHECK_EQ_INT(b.hp, 13);
    CHECK_EQ_INT(f.gameplay.score, 7000);
    f.orb.top = 64;
    f.pellets.slots[0].cloud_frame = 12;
    f.lasers.slots[4].alive = true;
    f.lasers.slots[4].origin_left = 100 * 256;
    for (int32_t frame = 0; frame < 5000 && !b.finished; frame++) {
        const bool attack = (b.phase == 2 && b.hp > 10)
            || (b.phase == 5 && b.hp > 6) || (b.phase == 6 && b.hp > 0);
        f.orb.left = attack ? 300 : 0;
        const bool completed = B15J_UpdateAndRender(&b, &f.context);
        if (!completed) {
            const B15J frozen = b;
            CHECK(!B15J_UpdateAndRender(&b, &f.context));
            CHECK(memcmp(&b, &frozen, sizeof(b)) == 0);
        }
        Frame_VSync();
    }
    CHECK(b.finished);
    CHECK_EQ_INT(b.hp, 0);
    CHECK_EQ_INT(f.gameplay.score, 98000);
    CHECK(f.cleared);
    CHECK(f.player.is_hit);
    CHECK(f.scene.active == &f.assets.next_scene);
    CHECK_EQ_INT(f.scene.active->id, 6);
    CHECK_EQ_INT(f.fades, 1);
    CHECK_EQ_INT(f.pellets.slots[0].cloud_frame, 12);
    CHECK(f.lasers.slots[4].alive);
    const B15J finished = b;
    CHECK(B15J_UpdateAndRender(&b, &f.context));
    CHECK(memcmp(&b, &finished, sizeof(b)) == 0);
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
        f.display.rank = rank;
        B15J b = {};
        B15J_Setup(&b, &f.context);
        b.phase = 6;
        b.subphase = 1;
        b.frame = 249;
        f.lasers.slots[0] =
            (SHOOTOUT_LASER) { .alive = true, .age = 77, .origin_left = 3210 };
        uint32_t expected = f.rng;
        MasterRng_Next(&expected);
        Frame_Reset();
        CHECK(B15J_UpdateAndRender(&b, &f.context));
        CHECK_EQ_INT(b.pattern_state, 52 + rank * 4);
        CHECK_EQ_INT(f.rng, expected);
        CHECK_EQ_INT(f.lasers.slots[0].age, 77);
        CHECK_EQ_INT(f.lasers.slots[0].origin_left, 3210);
        CHECK_EQ_INT(f.effects[6], 1);
        b.subphase = 2;
        b.frame = Math16_Wrap(b.pattern_state * 2 - 1);
        CHECK(B15J_UpdateAndRender(&b, &f.context));
        int32_t alive = 0;
        for (int32_t i = 0; i < PELLETS_COUNT; i++) {
            alive += f.pellets.slots[i].moving;
        }
        CHECK_EQ_INT(alive, 12);
        CHECK_EQ_INT(f.effects[7], 2);
        CHECK_EQ_INT(b.pattern_state, 52 + rank * 4);
        b.subphase = 3;
        b.frame = 99;
        MasterRng_Next(&expected);
        CHECK(B15J_UpdateAndRender(&b, &f.context));
        CHECK_EQ_INT(f.rng, expected);
        CHECK_EQ_INT(b.pattern_state, 60 + rank * 4);
        CHECK_EQ_INT(
            b.random_range,
            rank == 0       ? 0
                : rank == 1 ? 10
                : rank == 2 ? 16
                            : 20);
        CHECK_EQ_INT(f.lasers.slots[0].age, 77);
        Arena_Free(&arena);
    }
}

TEST(timeout_forces_one_hp_before_damage_without_a_health_draw)
{
    ARENA arena;
    Arena_Init(&arena, "b15j", 2 * 1024 * 1024);
    FIXTURE f;
    M_Battle(&f, &arena);
    B15J b = {};
    B15J_Setup(&b, &f.context);
    b.phase = 6;
    b.subphase = 1;
    b.patterns_done = 30;
    const HUD_HP health = f.health;
    Frame_Reset();
    CHECK(B15J_UpdateAndRender(&b, &f.context));
    CHECK_EQ_INT(b.hp, 1);
    CHECK(memcmp(&f.health, &health, sizeof(health)) == 0);
    CHECK_EQ_INT(f.gameplay.score, 0);
    f.test_damage = true;
    CHECK(!B15J_UpdateAndRender(&b, &f.context));
    CHECK_EQ_INT(b.hp, 0);
    CHECK(!f.test_damage);
    CHECK(b.defeating);
    CHECK_EQ_INT(f.fades, 1);
    CHECK_EQ_INT(f.gameplay.score, 0);
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
        B15J b = {};
        B15J_Setup(&b, &f.context);
        b.phase = 6;
        b.subphase = 1;
        b.tear_frame[0] = 1;
        b.tears[0].left = 37;
        b.tears[0].top = 392;
        f.player.invincible = false;
        f.player.left = positions[i];
        Frame_Reset();
        CHECK(B15J_UpdateAndRender(&b, &f.context));
        CHECK_EQ_INT(b.tear_frame[0], 2);
        CHECK_EQ_INT(f.player.is_hit, i == 1 || i == 2);
        CHECK_EQ_INT(f.effects[7], 1);
        Arena_Free(&arena);
    }
}

TEST(context_requires_complete_borrowed_resources)
{
    ARENA arena;
    Arena_Init(&arena, "b15j", 2 * 1024 * 1024);
    FIXTURE f;
    M_Battle(&f, &arena);
    CHECK(Result_Ignore(B15J_ValidateContext(&f.context)));
    f.context.fade_music = nullptr;
    CHECK(!Result_Ignore(B15J_ValidateContext(&f.context)));
    f.context.fade_music = M_Fade;
    f.hit_context.shots_output.action = nullptr;
    CHECK(!Result_Ignore(B15J_ValidateContext(&f.context)));
    f.hit_context.shots_output.action = M_Shot;
    f.display.remaining_lives = nullptr;
    CHECK(!Result_Ignore(B15J_ValidateContext(&f.context)));
    f.display.remaining_lives = &f.lives;
    f.assets.ripples.pixels = nullptr;
    CHECK(!Result_Ignore(B15J_ValidateContext(&f.context)));
    Arena_Free(&arena);
}

TEST(cloud_policy_is_preserved_when_rings_and_spreads_spawn)
{
    for (int32_t cloud = 0; cloud < 2; cloud++) {
        ARENA arena;
        Arena_Init(&arena, "b15j", 2 * 1024 * 1024);
        FIXTURE f;
        M_Battle(&f, &arena);
        B15J b = {};
        B15J_Setup(&b, &f.context);
        b.phase = 4;
        b.subphase = 1;
        b.frame = 79;
        b.souls[0].left = 100;
        b.souls[1].left = 500;
        b.souls[0].top = b.souls[1].top = 100;
        f.pellets.spawn_with_cloud = cloud != 0;
        const size_t used = arena.used;
        Frame_Reset();
        CHECK(B15J_UpdateAndRender(&b, &f.context));
        int32_t count = 0;
        for (int32_t i = 0; i < PELLETS_COUNT; i++) {
            count += cloud != 0 ? f.pellets.slots[i].cloud_frame == 1
                                : f.pellets.slots[i].moving;
        }
        CHECK_EQ_INT(count, 12);
        CHECK_EQ_INT(f.pellets.spawn_with_cloud, cloud != 0);
        CHECK(f.pellets.interlace);
        CHECK_EQ_INT(arena.used, used);
        Arena_Free(&arena);
    }
}
