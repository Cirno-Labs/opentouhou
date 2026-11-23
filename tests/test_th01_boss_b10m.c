#include "th01/boss_b10m.h"
#include "th01/session.h"
#include "pc98/zun/master_rng.h"
#include "pc98/zun/math16.h"

#include "core/base/arena.h"

#include "th01_video.h"
#include "test.h"

typedef struct {
    PALETTE_STATE palette;
    PALETTE stage_palette;
    B10M_ASSETS assets;
    BOSS_SPRITES bank;
    HUD_HP health;
    PARTICLES particles;
    HUD_CONTEXT display;
    BOSS_HIT_CONTEXT hit_context;
    B10M_CONTEXT context;
    MISSILES missiles;
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
    f->hit_context = (BOSS_HIT_CONTEXT) { .palette = &f->palette,
                                          .display = &f->display,
                                          .health = &f->health };
    f->context = (B10M_CONTEXT) { .assets = &f->assets,
                                  .sprites = &f->bank,
                                  .hit = &f->hit_context,
                                  .stage_palette = &f->stage_palette,
                                  .particles = &f->particles,
                                  .missiles = &f->missiles };
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
    f->rng = 0xb10a0000;
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
    g_Session.settings.difficulty = f->display.rank;
}

static void M_Enter(B10M *const b, FIXTURE *const f)
{
    for (int32_t frame = 0; frame < 331; frame++) {
        f->hit_context.frame_rand++;
        CHECK(B10M_UpdateAndRender(b, &f->context));
    }
    CHECK_EQ_INT(b->phase, B10M_PHASE_LATERAL);
    CHECK_EQ_INT(b->frame, 0);
}

TEST(setup_shares_eye_assets_and_retains_static_history)
{
    ARENA arena;
    Arena_Init(&arena, "b10m", 2 * 1024 * 1024);
    FIXTURE f;
    M_Init(&f, &arena);
    B10M b = { .frame = 77,
               .phase = B10M_PHASE_STAR_ATTACK,
               .pattern_state = 17,
               .subphase = 8,
               .hit = { 39, true },
               .hp_bar_filled = true,
               .pentagram = { .angle = 450, .phase = B10M_STAR_GROWN_SLAM } };
    for (int32_t i = 0; i < 5; i++) {
        b.eyes[i].lock_frame = (int16_t)(i % 4);
        b.eyes[i].previous_left = (int16_t)(i * 11);
        f.missiles.slots[i].flag = MISSILE_MOVING;
        f.missiles.slots[i].previous_top = (int16_t)(i * 9);
    }
    memset(g_Video.pages[0], 3, VIDEO_PAGE_SIZE);
    memset(g_Video.pages[1], 5, VIDEO_PAGE_SIZE);
    const size_t used = arena.used;
    B10M_Setup(&b, &f.context);
    CHECK_EQ_INT(arena.used, used);
    CHECK_EQ_INT(b.hp, 16);
    CHECK_EQ_INT(b.phase, B10M_PHASE_ENTRANCE);
    CHECK_EQ_INT(b.frame, 0);
    CHECK_EQ_INT(b.hit.frame, 39);
    CHECK(b.hit.invincible);
    CHECK(b.hp_bar_filled);
    CHECK_EQ_INT(b.pattern_state, 17);
    CHECK_EQ_INT(b.subphase, 8);
    CHECK_EQ_INT(b.pentagram.angle, 450);
    CHECK_EQ_INT(f.health.first_white, 12);
    CHECK_EQ_INT(f.health.first_redwhite, 8);
    CHECK(f.bank.entities[0] == &f.assets.eyes);
    const int16_t left[] = { 64, 512, 192, 384, 288 };
    const int16_t top[] = { 128, 128, 176, 176, 64 };
    for (int32_t i = 0; i < 5; i++) {
        CHECK_EQ_INT(b.eyes[i].left, left[i]);
        CHECK_EQ_INT(b.eyes[i].top, top[i]);
        CHECK_EQ_INT(b.eyes[i].sprites.slot, 0);
        CHECK_EQ_INT(b.eyes[i].sprites.width_bytes, 8);
        CHECK_EQ_INT(b.eyes[i].sprites.height, 48);
        CHECK_EQ_INT(b.eyes[i].image, 0);
        CHECK(b.eyes[i].orb_inactive);
        CHECK_EQ_INT(b.eyes[i].lock_frame, i % 4);
        CHECK_EQ_INT(b.eyes[i].previous_left, i * 11);
        CHECK_EQ_INT(f.missiles.slots[i].flag, MISSILE_FREE);
        CHECK_EQ_INT(f.missiles.slots[i].previous_top, i * 9);
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
    M_Init(&f, &arena);
    CHECK(Result_Ignore(B10M_ValidateAssets(&f.assets)));
    B10M_ASSETS a = f.assets;
    a.eyes.count = 6;
    CHECK(!Result_Ignore(B10M_ValidateAssets(&a)));
    a = f.assets;
    a.next_scene.id = 2;
    CHECK(!Result_Ignore(B10M_ValidateAssets(&a)));
    a = f.assets;
    a.missiles.count = 4;
    CHECK(!Result_Ignore(B10M_ValidateAssets(&a)));
    Arena_Free(&arena);
}

TEST(ordinary_entrance_preserves_world_and_rank_fire)
{
    for (int32_t rank = 0; rank < 4; rank++) {
        ARENA arena;
        Arena_Init(&arena, "b10m", 2 * 1024 * 1024);
        FIXTURE f;
        M_Battle(&f, &arena);
        f.display.rank = rank;
        g_Session.settings.difficulty = rank;
        B10M b = {};
        B10M_Setup(&b, &f.context);
        const size_t used = arena.used;
        Frame_Reset();
        const ORB orb = f.orb;
        M_Enter(&b, &f);
        CHECK_EQ_INT(g_VSync.count1, 0);
        CHECK(memcmp(&orb, &f.orb, sizeof(orb)) == 0);
        CHECK(!f.player.is_hit);
        CHECK_EQ_INT(f.gameplay.score, 0);
        CHECK_EQ_INT(arena.used, used);
        CHECK_EQ_INT(
            b.pattern_interval,
            rank == 0       ? 350
                : rank == 1 ? 300
                : rank == 2 ? 200
                            : 130);
        int32_t count = 0;
        for (int32_t i = 0; i < PELLETS_COUNT; i++) {
            count += f.pellets.slots[i].moving;
        }
        CHECK_EQ_INT(count, rank == 2 ? 15 : rank == 3 ? 25 : 0);
        CHECK_EQ_INT(b.eyes[0].image, 4);
        CHECK_EQ_INT(b.eyes[1].image, 3);
        CHECK(!b.eyes[0].orb_inactive);
        CHECK(b.eyes[4].orb_inactive);
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
        f.display.rank = rank;
        g_Session.settings.difficulty = rank;
        B10M b = {};
        B10M_Setup(&b, &f.context);
        Frame_Reset();
        const size_t used = arena.used;
        uint16_t phases = 0, star_phases = 0;
        int32_t loops = 0;
        int16_t previous_star_phase = -1;
        for (int32_t frame = 0; frame < 18000 && loops < 2; frame++) {
            f.player.left = (int16_t)(40 + (frame * 7) % 550);
            f.hit_context.frame_rand++;
            phases |= (uint16_t)(1u << b.phase);
            if (b.phase == B10M_PHASE_STAR_ATTACK) {
                star_phases |= (uint16_t)(1u << b.pentagram.phase);
            }
            const int8_t previous = b.phase;
            if (b.phase == B10M_PHASE_STAR_ATTACK) {
                previous_star_phase = b.pentagram.phase;
            }
            CHECK(B10M_UpdateAndRender(&b, &f.context));
            if (previous == B10M_PHASE_STAR_ATTACK
                && b.phase == B10M_PHASE_TO_STAR) {
                CHECK_EQ_INT(
                    b.pentagram.phase,
                    loops == 0 ? B10M_STAR_RETURN : B10M_STAR_WAIT);
                CHECK_EQ_INT(previous_star_phase, loops == 0 ? 6 : 9);
                loops++;
            }
        }
        CHECK_EQ_INT(phases, 0x3fff);
        CHECK_EQ_INT(star_phases, 0x3ff);
        CHECK_EQ_INT(loops, 2);
        CHECK_EQ_INT(b.hp, 16);
        CHECK_EQ_INT(arena.used, used);
        CHECK(f.rng != 0xb10a0000);
        CHECK_EQ_INT(f.fades, 0);
        CHECK(!b.finished);
        CHECK(f.scene.active == nullptr);
        Arena_Free(&arena);
    }
}

TEST(missile_variation_consumes_rng_with_a_full_pool)
{
    ARENA arena;
    Arena_Init(&arena, "b10m", 2 * 1024 * 1024);
    FIXTURE f;
    M_Battle(&f, &arena);
    B10M b = {};
    B10M_Setup(&b, &f.context);
    b.phase = B10M_PHASE_MISSILES;
    b.frame = 99;
    for (int32_t i = 0; i < MISSILES_COUNT; i++) {
        f.missiles.slots[i] = (MISSILE) { .left = 200 * 16,
                                          .top = 200 * 16,
                                          .previous_left = -1,
                                          .flag = MISSILE_MOVING };
    }
    uint32_t expected = f.rng;
    const int16_t variation = (int16_t)(MasterRng_Next(&expected) % 3);
    Frame_Reset();
    CHECK(B10M_UpdateAndRender(&b, &f.context));
    CHECK_EQ_INT(b.subphase, variation * 4 + 1);
    CHECK_EQ_INT(f.rng, expected);
    b.frame = 103;
    CHECK(B10M_UpdateAndRender(&b, &f.context));
    for (int32_t i = 0; i < MISSILES_COUNT; i++) {
        CHECK_EQ_INT(f.missiles.slots[i].left, 200 * 16);
        CHECK_EQ_INT(f.missiles.slots[i].top, 200 * 16);
    }
    CHECK_EQ_INT(f.rng, expected);
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
        B10M b = {};
        B10M_Setup(&b, &f.context);
        b.phase = (int8_t)phase;
        b.hp = 0;
        b.pattern_interval = 24;
        b.iterations = 1;
        if (variant == 2) {
            b.pentagram.phase = B10M_STAR_SLAM;
            b.pentagram.center.y = 420;
            b.frame = 3;
        }
        f.missiles.slots[0] = (MISSILE) { .flag = MISSILE_MOVING,
                                          .left = 200 * 16,
                                          .top = 200 * 16,
                                          .previous_left = -1,
                                          .velocity = { 16, 0 } };
        f.pellets.slots[0] =
            (PELLET) { .moving = true, .left = 200 * 16, .top = 200 * 16 };
        f.pellets.slots[1].cloud_frame = 10;
        f.lasers.slots[6].alive = true;
        Frame_Reset();
        CHECK(!B10M_UpdateAndRender(&b, &f.context));
        CHECK(b.defeating);
        CHECK_EQ_INT(f.fades, 1);
        CHECK(!f.cleared);
        CHECK(f.scene.active == nullptr);
        CHECK_EQ_INT(f.missiles.slots[0].left, 201 * 16);
        CHECK_EQ_INT(
            f.missiles.slots[0].flag,
            phase == B10M_PHASE_STAR_ATTACK ? MISSILE_FREE : MISSILE_MOVING);
        CHECK(!f.pellets.slots[0].moving);
        CHECK_EQ_INT(f.pellets.slots[1].cloud_frame, 10);
        CHECK(f.lasers.slots[6].alive);
        const B10M frozen = b;
        CHECK(!B10M_UpdateAndRender(&b, &f.context));
        CHECK(memcmp(&b, &frozen, sizeof(b)) == 0);
        const size_t used = arena.used;
        for (int32_t frame = 0; frame < 400 && !b.finished; frame++) {
            Frame_VSync();
            B10M_UpdateAndRender(&b, &f.context);
        }
        CHECK(b.finished);
        CHECK(f.cleared);
        CHECK(f.player.is_hit);
        CHECK(f.scene.active == &f.assets.next_scene);
        CHECK_EQ_INT(f.scene.active->id, 3);
        CHECK_EQ_INT(f.missiles.slots[0].left, 201 * 16);
        CHECK_EQ_INT(f.fades, 1);
        CHECK_EQ_INT(arena.used, used);
        const B10M finished = b;
        CHECK(B10M_UpdateAndRender(&b, &f.context));
        CHECK(memcmp(&b, &finished, sizeof(b)) == 0);
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
        B10M b = {};
        B10M_Setup(&b, &f.context);
        b.phase = phases[i];
        b.hp = hp[i];
        b.pattern_interval = 24;
        b.eyes[0].image = 3;
        b.eyes[0].orb_inactive = false;
        f.orb.previous_left = 88;
        f.orb.previous_top = 148;
        f.orb.velocity_x = ORB_X_LEFT_4;
        Frame_Reset();
        CHECK(B10M_UpdateAndRender(&b, &f.context));
        CHECK_EQ_INT(b.hp, hp[i] - 1);
        CHECK_EQ_INT(b.phase, phases[i] + 1);
        CHECK_EQ_INT(f.gameplay.score, 5000);
        CHECK_EQ_INT(f.orb.velocity_x, ORB_X_RIGHT_4);
        CHECK_EQ_INT(f.effects[3], 1);
        CHECK(b.hit.invincible);
        CHECK_EQ_INT(b.frame, 0);
        CHECK_EQ_INT(b.hit.frame, 0);
        const int16_t old_hp = b.hp;
        CHECK(B10M_UpdateAndRender(&b, &f.context));
        CHECK_EQ_INT(b.hp, old_hp);
        CHECK_EQ_INT(f.gameplay.score, 5000);
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
        B10M b = {};
        B10M_Setup(&b, &f.context);
        b.frame = 160;
        b.hp_bar_filled = true;
        f.player.invincible = false;
        f.player.left = positions[i];
        Frame_Reset();
        CHECK(B10M_UpdateAndRender(&b, &f.context));
        CHECK_EQ_INT(f.player.is_hit, i == 0 || i == 2);
        CHECK_EQ_INT(Th01Test_Pixel(0, 92, 156), 7);
        CHECK_EQ_INT(Th01Test_Pixel(0, 540, 398), 7);
        CHECK_EQ_INT(Th01Test_Pixel(0, 540, 399), 0);
        f.player.invincible = true;
        CHECK(B10M_UpdateAndRender(&b, &f.context));
        CHECK(!f.player.is_hit);
        Arena_Free(&arena);
    }
}

TEST(palette_recolor_keeps_signed_overflow_and_dac_clamp)
{
    ARENA arena;
    Arena_Init(&arena, "b10m", 2 * 1024 * 1024);
    FIXTURE f;
    M_Battle(&f, &arena);
    B10M b = {};
    B10M_Setup(&b, &f.context);
    b.phase = B10M_PHASE_STAR_RISE;
    b.pentagram.center = (MATH16_VECTOR) { 316, 240 };
    b.pentagram.radius = 64;
    f.stage_palette.colors[15][0] = 126;
    f.stage_palette.colors[15][1] = 0;
    f.stage_palette.colors[15][2] = 5;
    Palette_Set(&f.palette, &f.stage_palette);
    Frame_Reset();
    for (int32_t i = 0; i < 70; i++) {
        CHECK(B10M_UpdateAndRender(&b, &f.context));
    }
    CHECK_EQ_INT(f.stage_palette.colors[15][0], -67);
    CHECK_EQ_INT(f.palette.saved.colors[15][0], 0);
    CHECK_EQ_INT(f.palette.saved.colors[15][2], 5);
    CHECK_EQ_INT(b.phase, B10M_PHASE_STAR_ATTACK);
    CHECK_EQ_INT(b.pentagram.center.y, 189);
    CHECK_EQ_INT(b.pentagram.angle, 0);
    CHECK_EQ_INT(b.pattern_interval, 0);
    Arena_Free(&arena);
}
