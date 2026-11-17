#include "th01/boss_b15m.h"

#include "core/base/arena.h"

#include "test.h"

typedef struct {
    VIDEO video;
    PALETTE_STATE palette;
    B15M_ASSETS assets;
    BOSS_SPRITES bank;
    HUD_HP health;
    PARTICLES particles;
    HUD_CONTEXT display;
    BOSS_HIT_CONTEXT hit;
    B15M_CONTEXT context;
    PELLETS pellets;
    MISSILES missiles;
    SHOOTOUT_LASERS lasers;
    PELLET_SHARED shared;
    HUD hud;
    PLAYER player;
    ORB orb;
    SHOTS shots;
    STAGE_GAMEPLAY gameplay;
    GAME_TEXT_FONT font;
    PTN stage, miko;
    SCENE_STATE scene;
    uint32_t high_score;
    int16_t lives;
    bool test_damage, cleared;
    int32_t effects[17], fades;
    uint32_t rng;
} FIXTURE;

static void M_Init(FIXTURE *const f, ARENA *const arena)
{
    *f = (FIXTURE) {};
    const ALLOCATOR *const a = Arena_Allocator(arena);
    Video_Init(&f->video, a);
    Palette_Init(&f->palette);
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
    f->display.video = &f->video;
    f->hit = (BOSS_HIT_CONTEXT) { .display = &f->display,
                                  .palette = &f->palette,
                                  .health = &f->health,
                                  .frame_rand = 0x12345678 };
    f->context = (B15M_CONTEXT) { .assets = &f->assets,
                                  .sprites = &f->bank,
                                  .hit = &f->hit,
                                  .rng = &f->rng,
                                  .particles = &f->particles,
                                  .pellets = &f->pellets,
                                  .missiles = &f->missiles };
}

TEST(setup_reseeds_without_resetting_actor_or_missile_history)
{
    ARENA arena;
    Arena_Init(&arena, "b15m", 2 * 1024 * 1024);
    FIXTURE f;
    M_Init(&f, &arena);
    B15M b;
    B15M_Init(&b, Arena_Allocator(&arena));
    memset(b.background, 0, 128 * 96);
    b.hit = (BOSS_HIT) { 27, true };
    b.pattern_state = 37;
    b.girl.image = 4;
    b.attack.orb_hitbox = (BOSS_BOUNDS) { 1, 2, 3, 4 };
    b.bat.lock_frame = 7;
    b.girl.previous_left = 199;
    b.girl.orb_inactive = true;
    b.missile_rifts.x[0] = 88;
    f.missiles.slots[0].flag = MISSILE_MOVING;
    f.missiles.slots[0].left = 300;
    const MISSILES missiles = f.missiles;
    const PALETTE saved = f.palette.saved;
    const size_t used = arena.used;
    B15M_Setup(&b, &f.context);
    CHECK_EQ_INT(arena.used, used);
    CHECK_EQ_INT(b.hp, 14);
    CHECK_EQ_INT(b.phase, 0);
    CHECK_EQ_INT(b.frame, 0);
    CHECK_EQ_INT(f.health.first_white, 10);
    CHECK_EQ_INT(f.health.first_redwhite, 6);
    CHECK_EQ_INT(f.rng, 0x12345678);
    CHECK(f.pellets.interlace);
    CHECK(memcmp(&f.missiles, &missiles, sizeof(missiles)) == 0);
    CHECK(memcmp(&f.palette.saved, &saved, sizeof(saved)) == 0);
    CHECK_EQ_INT(b.girl.left, 256);
    CHECK_EQ_INT(b.girl.top, 96);
    CHECK_EQ_INT(b.attack.left, 256);
    CHECK_EQ_INT(b.bat.top, 96);
    CHECK_EQ_INT(b.girl.image, 4);
    CHECK_EQ_INT(b.girl.previous_left, 199);
    CHECK(b.girl.orb_inactive);
    CHECK_EQ_INT(b.bat.lock_frame, 7);
    CHECK_EQ_INT(b.hit.frame, 27);
    CHECK(b.hit.invincible);
    CHECK_EQ_INT(b.pattern_state, 37);
    CHECK_EQ_INT(b.missile_rifts.x[0], 88);
    CHECK_EQ_INT(b.girl.orb_hitbox.left, 32);
    CHECK_EQ_INT(b.girl.orb_hitbox.right, 96);
    CHECK_EQ_INT(b.girl.orb_hitbox.top, 12);
    CHECK_EQ_INT(b.girl.orb_hitbox.bottom, 64);
    CHECK_EQ_INT(b.bat.orb_hitbox.left, 8);
    CHECK_EQ_INT(b.bat.orb_hitbox.right, 40);
    CHECK_EQ_INT(b.bat.orb_hitbox.top, 8);
    CHECK_EQ_INT(b.bat.orb_hitbox.bottom, 24);
    CHECK_EQ_INT(b.attack.orb_hitbox.left, 1);
    CHECK_EQ_INT(b.attack.orb_hitbox.bottom, 4);
    CHECK(f.bank.entities[0] == &f.assets.girl);
    CHECK(f.bank.entities[1] == &f.assets.attack);
    CHECK(f.bank.entities[2] == &f.assets.bat);
    const PALETTE black = {};
    CHECK(memcmp(&b.post_defeat, &black, sizeof(black)) == 0);
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
        Shots_RenderAction(&f->video, &f->stage, action);
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
    f->display = (HUD_CONTEXT) { .video = &f->video,
                                 .sprites = &f->stage,
                                 .font = &f->font,
                                 .gameplay = &f->gameplay,
                                 .player = &f->player,
                                 .high_score = &f->high_score,
                                 .remaining_lives = &f->lives };
    f->hit.hud = &f->hud;
    f->hit.shots = &f->shots;
    f->hit.shots_output = (SHOTS_OUTPUT) { M_Shot, f };
    f->hit.orb = &f->orb;
    f->hit.test_damage = &f->test_damage;
    f->hit.effect = M_Effect;
    f->hit.user = f;
    f->context.pellets = &f->pellets;
    f->context.pellet_shared = &f->shared;
    f->context.lasers = &f->lasers;
    f->context.rng = &f->rng;
    f->context.miko = &f->miko;
    f->context.scene = &f->scene;
    f->context.stage_cleared = &f->cleared;
    f->context.fade_music = M_Fade;
    f->context.missiles = &f->missiles;
    CHECK(Result_Ignore(B15M_ValidateContext(&f->context)));
}

static void M_Run(
    B15M *const b, FIXTURE *const f, FRAME_VSYNC *const vsync,
    uint64_t *const tick)
{
    int32_t polls = 0;
    while (!B15M_UpdateAndRender(b, &f->context, vsync, *tick)) {
        CHECK(polls++ < 2000);
        if (polls >= 2000) {
            break;
        }
        (*tick)++;
        Frame_VSync(vsync);
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
        B15M b;
        B15M_Init(&b, Arena_Allocator(&arena));
        memset(b.background, 0, 128 * 96);
        B15M_Setup(&b, &f.context);
        f.missiles.slots[0] = (MISSILE) { .left = 200 * 16,
                                          .top = 200 * 16,
                                          .previous_left = -1,
                                          .velocity = { 16, 0 },
                                          .flag = MISSILE_MOVING };
        FRAME_VSYNC v = { .count1 = initial[i], .count2 = 9 };
        uint64_t tick = 0;
        CHECK(!B15M_UpdateAndRender(&b, &f.context, &v, tick));
        CHECK(b.entrance_active);
        CHECK_EQ_INT(f.missiles.slots[0].left, 201 * 16);
        const B15M frozen = b;
        const uint32_t rng = f.rng;
        CHECK(!B15M_UpdateAndRender(&b, &f.context, &v, tick));
        CHECK(memcmp(&b, &frozen, sizeof(b)) == 0);
        CHECK_EQ_INT(f.rng, rng);
        const size_t used = arena.used;
        M_Run(&b, &f, &v, &tick);
        CHECK_EQ_INT(tick, expected[i]);
        CHECK_EQ_INT(v.count2, 9 + expected[i]);
        CHECK_EQ_INT(v.count1, 0);
        CHECK_EQ_INT(b.phase, 1);
        CHECK_EQ_INT(b.frame, 0);
        CHECK_EQ_INT(b.hit.frame, 0);
        CHECK_EQ_INT(b.pattern, 1);
        CHECK(!b.entrance_active);
        CHECK(!b.girl.orb_inactive);
        CHECK_EQ_INT(f.missiles.slots[0].left, 201 * 16);
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
    B15M b;
    B15M_Init(&b, Arena_Allocator(&arena));
    memset(b.background, 0, 128 * 96);
    B15M_Setup(&b, &f.context);
    b.phase = 4;
    b.frame = 19;
    b.teleport_done = true;
    b.hit.frame = 17;
    FRAME_VSYNC v = { .count1 = 12, .count2 = 25 };
    CHECK(!B15M_UpdateAndRender(&b, &f.context, &v, 0));
    CHECK_EQ_INT(b.pending, 1);
    CHECK_EQ_INT(b.frame, 20);
    const B15M frozen = b;
    const uint32_t rng = f.rng;
    CHECK(!B15M_UpdateAndRender(&b, &f.context, &v, 0));
    CHECK(memcmp(&b, &frozen, sizeof(b)) == 0);
    CHECK_EQ_INT(f.rng, rng);
    Frame_VSync(&v);
    CHECK(B15M_UpdateAndRender(&b, &f.context, &v, 1));
    CHECK_EQ_INT(b.phase, 4);
    CHECK_EQ_INT(b.pattern, 0);
    CHECK_EQ_INT(b.frame, 20);
    CHECK_EQ_INT(b.hit.frame, 17);
    CHECK_EQ_INT(v.count1, 13);
    CHECK_EQ_INT(v.count2, 26);
    CHECK_EQ_INT(f.video.scroll_top, 0);
    Arena_Free(&arena);
}

TEST(safety_circle_delays_hit_and_preserves_later_source_effects)
{
    ARENA arena;
    Arena_Init(&arena, "b15m", 4 * 1024 * 1024);
    FIXTURE f;
    M_Battle(&f, &arena);
    B15M b;
    B15M_Init(&b, Arena_Allocator(&arena));
    memset(b.background, 0, 128 * 96);
    B15M_Setup(&b, &f.context);
    b.phase = 5;
    b.form = 0;
    b.frame = 140;
    b.girl_pattern = 2;
    b.girl_subphase = 1;
    b.pattern_state = 2;
    b.safety_circle = (B15M_CIRCLE) { 64, 1 };
    b.safety_left = 200;
    f.player.left = 400;
    f.player.invincible = false;
    FRAME_VSYNC v = {};
    CHECK(!B15M_UpdateAndRender(&b, &f.context, &v, 100));
    CHECK_EQ_INT(b.pending, 2);
    CHECK_EQ_INT(b.safety_circle.frame, 2);
    CHECK(!f.player.is_hit);
    const B15M frozen = b;
    const uint32_t rng = f.rng;
    CHECK(!B15M_UpdateAndRender(&b, &f.context, &v, 105));
    CHECK(memcmp(&b, &frozen, sizeof(b)) == 0);
    CHECK_EQ_INT(f.rng, rng);
    CHECK(!f.player.is_hit);
    CHECK(B15M_UpdateAndRender(&b, &f.context, &v, 106));
    CHECK(f.player.is_hit);
    CHECK_EQ_INT(b.safety_circle.frame, 200);
    CHECK_EQ_INT(b.frame, 141);
    CHECK(f.pellets.slots[0].moving);
    CHECK_EQ_INT(b.girl_subphase, 1);
    CHECK_EQ_INT(b.phase5_subphase, 0);
    Arena_Free(&arena);
}

TEST(real_orb_battle_visits_every_form_and_pattern_then_defeats)
{
    for (int32_t rank = 0; rank < 4; rank++) {
        ARENA arena;
        Arena_Init(&arena, "b15m", 4 * 1024 * 1024);
        FIXTURE f;
        M_Battle(&f, &arena);
        f.display.rank = rank;
        f.hit.frame_rand = 0xb15f0000u + (uint32_t)rank;
        B15M b;
        B15M_Init(&b, Arena_Allocator(&arena));
        memset(b.background, 0, 128 * 96);
        B15M_Setup(&b, &f.context);
        FRAME_VSYNC v = {};
        uint64_t tick = 0;
        M_Run(&b, &f, &v, &tick);
        const size_t used = arena.used;
        uint32_t phase1 = 0, phase3 = 0, girl = 0, bat = 0;
        int32_t p1ticks = 0, p3ticks = 0;
        for (int32_t step = 1; step < 40000 && !b.finished; step++) {
            f.hit.frame_rand = (uint32_t)step;
            if (b.phase == 1) {
                p1ticks++;
                if (b.pattern > 0 && b.pattern <= 3) {
                    phase1 |= 1u << b.pattern;
                }
            }
            if (b.phase == 3) {
                p3ticks++;
                if (b.pattern > 0 && b.pattern <= 3 && b.phase3_pattern != 0) {
                    phase3 |= 1u << b.pattern;
                }
            }
            if (b.phase == 5) {
                if (b.form == 0 && b.girl_pattern > 0 && b.girl_pattern <= 3) {
                    girl |= 1u << b.girl_pattern;
                }
                if (b.form == 1 && b.bat_pattern > 0 && b.bat_pattern <= 4) {
                    bat |= 1u << b.bat_pattern;
                }
            }
            int16_t target = b.hp;
            if (b.phase == 1 && phase1 == 14 && p1ticks > 2500) {
                target = 10;
            } else if (b.phase == 3 && phase3 == 14 && p3ticks > 2000) {
                target = 6;
            } else if (b.phase == 5 && girl == 14 && bat == 30) {
                target = 0;
            }
            const BOSS_ENTITY *const e = b.form == 0 ? &b.girl : &b.bat;
            const bool hit = b.hp > target && step % 50 == 20;
            f.orb.previous_left = hit ? e->left + (b.form ? 16 : 48) : 0;
            f.orb.previous_top = hit ? e->top + 24 : 399;
            M_Run(&b, &f, &v, &tick);
            Frame_VSync(&v);
            tick++;
        }
        CHECK_EQ_INT(phase1, 14);
        CHECK_EQ_INT(phase3, 14);
        CHECK_EQ_INT(girl, 14);
        CHECK_EQ_INT(bat, 30);
        CHECK(b.finished);
        CHECK_EQ_INT(b.hp, 0);
        CHECK_EQ_INT(f.gameplay.score, 98000);
        CHECK_EQ_INT(f.fades, 1);
        CHECK(f.cleared);
        CHECK(f.player.is_hit);
        CHECK(f.scene.active == &f.assets.next_scene);
        CHECK_EQ_INT(f.scene.active->id, 5);
        CHECK_EQ_INT(arena.used, used);
        const B15M done = b;
        CHECK(B15M_UpdateAndRender(&b, &f.context, &v, tick));
        CHECK(memcmp(&b, &done, sizeof(b)) == 0);
        Arena_Free(&arena);
    }
}

TEST(safety_hit_tail_stacks_real_defeat_without_repeating_the_prefix)
{
    ARENA arena;
    Arena_Init(&arena, "b15m", 4 * 1024 * 1024);
    FIXTURE f;
    M_Battle(&f, &arena);
    B15M b;
    B15M_Init(&b, Arena_Allocator(&arena));
    memset(b.background, 0, 128 * 96);
    B15M_Setup(&b, &f.context);
    b.phase = 5;
    b.hp = 1;
    b.frame = 140;
    b.form = 0;
    b.girl_pattern = 2;
    b.girl_subphase = 1;
    b.pattern_state = 2;
    b.safety_circle = (B15M_CIRCLE) { 64, 1 };
    b.safety_left = 200;
    f.player.left = 400;
    f.player.invincible = false;
    f.orb.previous_left = b.girl.left + 48;
    f.orb.previous_top = b.girl.top + 24;
    f.missiles.slots[0] = (MISSILE) { .left = 200 * 16,
                                      .top = 200 * 16,
                                      .previous_left = -1,
                                      .velocity = { 16, 0 },
                                      .flag = MISSILE_MOVING };
    f.pellets.slots[1].cloud_frame = 10;
    for (int32_t i = 0; i < 10; i++) {
        f.lasers.slots[i].alive = true;
    }
    FRAME_VSYNC v = {};
    const uint32_t rng = f.rng;
    const size_t used = arena.used;
    CHECK(!B15M_UpdateAndRender(&b, &f.context, &v, 100));
    CHECK_EQ_INT(b.pending, 2);
    CHECK(!f.player.is_hit);
    CHECK(!f.cleared);
    CHECK_EQ_INT(f.fades, 0);
    CHECK_EQ_INT(f.missiles.slots[0].left, 201 * 16);
    CHECK(!B15M_UpdateAndRender(&b, &f.context, &v, 105));
    CHECK(!f.player.is_hit);
    CHECK_EQ_INT(f.fades, 0);
    CHECK(!B15M_UpdateAndRender(&b, &f.context, &v, 106));
    CHECK(b.defeating);
    CHECK_EQ_INT(b.hp, 0);
    CHECK(f.player.is_hit);
    CHECK(!f.cleared);
    CHECK(f.scene.active == nullptr);
    CHECK_EQ_INT(f.gameplay.score, 7000);
    CHECK_EQ_INT(f.effects[3], 1);
    CHECK_EQ_INT(f.fades, 1);
    CHECK_EQ_INT(f.rng, rng);
    CHECK_EQ_INT(f.missiles.slots[0].flag, MISSILE_FREE);
    CHECK_EQ_INT(f.missiles.slots[0].left, 201 * 16);
    CHECK_EQ_INT(f.pellets.slots[1].cloud_frame, 10);
    for (int32_t i = 0; i < 10; i++) {
        CHECK(!f.lasers.slots[i].alive);
    }
    const B15M frozen = b;
    CHECK(!B15M_UpdateAndRender(&b, &f.context, &v, 106));
    CHECK(memcmp(&b, &frozen, sizeof(b)) == 0);
    uint64_t tick = 106;
    M_Run(&b, &f, &v, &tick);
    CHECK(b.finished);
    CHECK(f.cleared);
    CHECK(f.scene.active == &f.assets.next_scene);
    CHECK_EQ_INT(f.fades, 1);
    CHECK_EQ_INT(f.effects[3], 1);
    CHECK_EQ_INT(f.missiles.slots[0].left, 201 * 16);
    CHECK_EQ_INT(f.rng, rng);
    CHECK_EQ_INT(arena.used, used);
    Arena_Free(&arena);
}

TEST(phase_hp_gate_waits_for_teleport_and_preserves_unused_rng_draws)
{
    ARENA arena;
    Arena_Init(&arena, "b15m", 4 * 1024 * 1024);
    FIXTURE f;
    M_Battle(&f, &arena);
    B15M b;
    B15M_Init(&b, Arena_Allocator(&arena));
    memset(b.background, 0, 128 * 96);
    B15M_Setup(&b, &f.context);
    b.phase = 1;
    b.hp = 10;
    b.pattern = 0;
    b.frame = 79;
    b.hp_bar_filled = true;
    uint32_t expected = f.rng;
    for (int32_t i = 0; i < 12; i++) {
        Math_Random(&expected);
    }
    FRAME_VSYNC v = {};
    CHECK(B15M_UpdateAndRender(&b, &f.context, &v, 0));
    CHECK_EQ_INT(b.phase, 1);
    CHECK_EQ_INT(b.frame, 80);
    CHECK(!b.teleport_done);
    CHECK_EQ_INT(f.rng, expected);
    Math_Random(&expected);
    Math_Random(&expected);
    CHECK(B15M_UpdateAndRender(&b, &f.context, &v, 1));
    CHECK_EQ_INT(b.phase, 2);
    CHECK_EQ_INT(b.frame, 0);
    CHECK_EQ_INT(f.rng, expected);
    Arena_Free(&arena);
}

TEST(circle_groups_keep_the_callers_cloud_policy)
{
    for (int32_t cloud = 0; cloud < 2; cloud++) {
        ARENA arena;
        Arena_Init(&arena, "b15m", 4 * 1024 * 1024);
        FIXTURE f;
        M_Battle(&f, &arena);
        B15M b;
        B15M_Init(&b, Arena_Allocator(&arena));
        memset(b.background, 0, 128 * 96);
        B15M_Setup(&b, &f.context);
        b.phase = 1;
        b.pattern = 3;
        b.frame = 200;
        b.circle.frame = 40;
        b.circle.angle = 64;
        b.pattern_state = PELLET_GROUP_SINGLE_AIMED;
        b.hp_bar_filled = true;
        f.pellets.spawn_with_cloud = cloud != 0;
        FRAME_VSYNC v = {};
        const size_t used = arena.used;
        CHECK(B15M_UpdateAndRender(&b, &f.context, &v, 0));
        int32_t count = 0;
        for (int32_t i = 0; i < PELLETS_COUNT; i++) {
            count += cloud ? f.pellets.slots[i].cloud_frame != 0
                           : f.pellets.slots[i].moving;
        }
        CHECK_EQ_INT(count, 23);
        CHECK_EQ_INT(f.pellets.spawn_with_cloud, cloud != 0);
        CHECK(f.pellets.interlace);
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
    B15M b;
    B15M_Init(&b, Arena_Allocator(&arena));
    memset(b.background, 0, 128 * 96);
    B15M_Setup(&b, &f.context);
    b.phase = 1;
    b.pattern = 2;
    b.frame = 59;
    b.hp_bar_filled = true;
    b.pattern_state = 15;
    for (int32_t i = 0; i < MISSILES_COUNT; i++) {
        f.missiles.slots[i] = (MISSILE) { .left = 200 * 16,
                                          .top = 200 * 16,
                                          .previous_left = -1,
                                          .flag = MISSILE_MOVING };
    }
    FRAME_VSYNC v = {};
    CHECK(B15M_UpdateAndRender(&b, &f.context, &v, 0));
    b.frame = 62;
    uint32_t expected = f.rng;
    Math_Random(&expected);
    Math_Random(&expected);
    CHECK(B15M_UpdateAndRender(&b, &f.context, &v, 1));
    CHECK_EQ_INT(f.rng, expected);
    for (int32_t i = 0; i < MISSILES_COUNT; i++) {
        CHECK_EQ_INT(f.missiles.slots[i].flag, MISSILE_MOVING);
        CHECK_EQ_INT(f.missiles.slots[i].left, 200 * 16);
    }
    Arena_Free(&arena);
}

TEST(context_validation_requires_live_hud_and_audio_consumers)
{
    ARENA arena;
    Arena_Init(&arena, "b15m", 4 * 1024 * 1024);
    FIXTURE f;
    M_Battle(&f, &arena);
    CHECK(Result_Ignore(B15M_ValidateContext(&f.context)));
    B15M_CONTEXT bad = f.context;
    bad.fade_music = nullptr;
    CHECK(!Result_Ignore(B15M_ValidateContext(&bad)));
    bad = f.context;
    bad.lasers = nullptr;
    CHECK(!Result_Ignore(B15M_ValidateContext(&bad)));
    BOSS_HIT_CONTEXT hit = f.hit;
    hit.shots_output.action = nullptr;
    bad = f.context;
    bad.hit = &hit;
    CHECK(!Result_Ignore(B15M_ValidateContext(&bad)));
    Arena_Free(&arena);
}
