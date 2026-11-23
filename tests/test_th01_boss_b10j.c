#include "th01/boss_b10j.h"
#include "pc98/zun/master_rng.h"
#include "pc98/zun/math16.h"
#include "core/base/arena.h"
#include "test.h"
typedef struct {
    PALETTE_STATE palette;
    PALETTE stage_palette;
    B10J_ASSETS assets;
    BOSS_SPRITES bank;
    HUD_HP health;
    PARTICLES particles;
    HUD_CONTEXT display;
    BOSS_HIT_CONTEXT hit_context;
    B10J_CONTEXT context;
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
    f->assets.still = (BOS) { 128, 160, 1, Arena_AllocZ(arena, 128 * 160),
                              Arena_AllocZ(arena, 128 * 160) };
    f->assets.animation = (BOS) { 128, 64, 5, Arena_AllocZ(arena, 128 * 64 * 5),
                                  Arena_AllocZ(arena, 128 * 64 * 5) };
    f->assets.pillars = Arena_AllocZ(arena, sizeof(B10J_PILLARS));
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
    f->hit_context = (BOSS_HIT_CONTEXT) { .palette = &f->palette,
                                          .display = &f->display,
                                          .health = &f->health };
    f->context = (B10J_CONTEXT) { .assets = &f->assets,
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
    f->rng = 0xb10b0000;
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
    CHECK(Result_Ignore(B10J_ValidateContext(&f->context)));
}

TEST(setup_preserves_saved_palette_and_all_sixteen_waits)
{
    ARENA arena;
    Arena_Init(&arena, "b10j", 2 * 1024 * 1024);
    FIXTURE f;
    M_Init(&f, &arena);
    B10J b;
    B10J_Init(&b, Arena_Allocator(&arena));
    b.frame = 91;
    b.hp = 3;
    b.still.lock_frame = 2;
    b.still.previous_top = 27;
    b.hit.frame = 17;
    b.pattern_state = 99;
    f.missiles.slots[0].flag = MISSILE_MOVING;
    f.missiles.slots[0].previous_top = 36;
    Frame_Reset();
    g_VSync.count1 = 12;
    g_VSync.count2 = 19;
    B10J_BeginSetup(&b, &f.context);
    const size_t used = arena.used;
    CHECK(!B10J_ResumeSetup(&b, &f.context));
    CHECK_EQ_INT(b.frame, 91);
    CHECK_EQ_INT(b.hp, 3);
    for (int32_t step = 0; step < 16; step++) {
        Frame_VSync();
        if (step == 15) {
            CHECK(B10J_ResumeSetup(&b, &f.context));
        } else {
            CHECK(!B10J_ResumeSetup(&b, &f.context));
            const B10J frozen = b;
            CHECK(!B10J_ResumeSetup(&b, &f.context));
            CHECK(memcmp(&b, &frozen, sizeof(b)) == 0);
            CHECK_EQ_INT(b.hp, 3);
        }
        CHECK(
            memcmp(&f.palette.saved, &f.assets.palette, sizeof(PALETTE)) == 0);
        for (int32_t i = 0; i < 16; i++) {
            const int32_t target = f.assets.palette.colors[i][0];
            const int32_t faded = 14 - step;
            CHECK_EQ_INT(
                g_Video.palette[i].red, (faded > target ? faded : target) * 17);
        }
    }
    CHECK_EQ_INT(b.still.left, 256);
    CHECK_EQ_INT(b.still.top, 64);
    CHECK_EQ_INT(b.still.previous_top, 27);
    CHECK_EQ_INT(b.still.lock_frame, 2);
    CHECK_EQ_INT(b.hit.frame, 17);
    CHECK_EQ_INT(b.pattern_state, 99);
    CHECK_EQ_INT(b.hp, 12);
    CHECK_EQ_INT(b.frame, 0);
    CHECK_EQ_INT(b.phase, 0);
    CHECK_EQ_INT(f.health.first_white, 6);
    CHECK_EQ_INT(f.health.first_redwhite, 2);
    CHECK_EQ_INT(f.missiles.slots[0].flag, MISSILE_FREE);
    CHECK_EQ_INT(f.missiles.slots[0].previous_top, 36);
    CHECK_EQ_INT(g_VSync.count1, 28);
    CHECK_EQ_INT(g_VSync.count2, 35);
    CHECK_EQ_INT(arena.used, used);
    Arena_Free(&arena);
}

TEST(assets_require_both_banks_and_original_pillars)
{
    ARENA arena;
    Arena_Init(&arena, "b10j", 2 * 1024 * 1024);
    FIXTURE f;
    M_Init(&f, &arena);
    CHECK(Result_Ignore(B10J_ValidateAssets(&f.assets)));
    f.assets.animation.count = 4;
    CHECK(!Result_Ignore(B10J_ValidateAssets(&f.assets)));
    f.assets.animation.count = 5;
    f.assets.pillars = nullptr;
    CHECK(!Result_Ignore(B10J_ValidateAssets(&f.assets)));
    Arena_Free(&arena);
}

static void M_Setup(B10J *const b, FIXTURE *const f, ARENA *const arena)
{
    B10J_Init(b, Arena_Allocator(arena));
    B10J_BeginSetup(b, &f->context);
    while (!B10J_ResumeSetup(b, &f->context)) {
        Frame_VSync();
    }
}

static void M_Enter(B10J *const b, FIXTURE *const f)
{
    for (int32_t edge = 0; edge < 110; edge++) {
        if (B10J_UpdateAndRender(b, &f->context)) {
            CHECK_EQ_INT(b->phase, 1);
            CHECK_EQ_INT(b->frame, 0);
            return;
        }
        Frame_VSync();
    }
    TEST_FAIL("entrance did not complete");
}

TEST(entrance_preserves_prefix_and_requires_every_delay)
{
    ARENA arena;
    Arena_Init(&arena, "b10j", 2 * 1024 * 1024);
    FIXTURE f;
    M_Battle(&f, &arena);
    Frame_Reset();
    B10J b;
    M_Setup(&b, &f, &arena);
    g_VSync.count1 = 0;
    f.missiles.slots[0] = (MISSILE) { .flag = MISSILE_MOVING,
                                      .left = 200 * 16,
                                      .top = 200 * 16,
                                      .previous_left = -1,
                                      .velocity = { 16, 0 } };
    const size_t used = arena.used;
    CHECK(!B10J_UpdateAndRender(&b, &f.context));
    CHECK_EQ_INT(f.missiles.slots[0].left, 201 * 16);
    CHECK_EQ_INT(b.frame, 1);
    const B10J frozen = b;
    const uint32_t frozen_rng = f.rng;
    CHECK(!B10J_UpdateAndRender(&b, &f.context));
    CHECK(memcmp(&b, &frozen, sizeof(b)) == 0);
    CHECK_EQ_INT(f.rng, frozen_rng);
    for (int32_t edge = 1; edge <= 100; edge++) {
        Frame_VSync();
        const bool done = B10J_UpdateAndRender(&b, &f.context);
        CHECK(done == (edge == 100));
    }
    CHECK_EQ_INT(b.phase, 1);
    CHECK_EQ_INT(b.frame, 0);
    CHECK_EQ_INT(b.still.left, 256);
    CHECK_EQ_INT(b.still.top, 120);
    CHECK_EQ_INT(f.missiles.slots[0].left, 201 * 16);
    uint32_t expected = frozen_rng;
    MasterRng_Next(&expected);
    CHECK_EQ_INT(f.rng, expected);
    CHECK_EQ_INT(b.hop, 255);
    CHECK_EQ_INT(b.spread_interval, 4);
    CHECK_EQ_INT(b.spread_speed, 16);
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
        f.display.rank = rank;
        Frame_Reset();
        B10J b;
        M_Setup(&b, &f, &arena);
        M_Enter(&b, &f);
        const size_t used = arena.used;
        uint16_t patterns = 0;
        int32_t loops = 0;
        for (int32_t step = 0; step < 12000 && loops < 2; step++) {
            const int8_t phase = b.phase;
            const int16_t pattern = b.pattern;
            if ((phase == 1 || phase == 3) && pattern >= 0 && pattern < 4) {
                patterns |= (uint16_t)(1u << (pattern + (phase == 3 ? 4 : 0)));
            }
            if ((patterns & 15) == 15) {
                b.hp = 6;
            }
            f.hit_context.frame_rand++;
            CHECK(B10J_UpdateAndRender(&b, &f.context));
            if (phase == 3 && pattern == 3 && b.pattern == 0) {
                loops++;
            }
        }
        CHECK_EQ_INT(patterns, 255);
        CHECK_EQ_INT(loops, 2);
        CHECK_EQ_INT(b.hp, 6);
        CHECK_EQ_INT(f.fades, 0);
        CHECK(!b.finished);
        CHECK(f.scene.active == nullptr);
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
    Frame_Reset();
    B10J b;
    M_Setup(&b, &f, &arena);
    M_Enter(&b, &f);
    const size_t used = arena.used;
    bool saw_gate = false;
    for (int32_t step = 0; step < 5000 && !b.defeating; step++) {
        f.hit_context.frame_rand++;
        const bool colliding =
            step % 50 == 20 && (b.phase == 3 || (b.phase == 1 && b.hp > 6));
        f.orb.previous_left = colliding ? Math16_Wrap(b.still.left + 48) : 0;
        f.orb.previous_top = colliding ? Math16_Wrap(b.still.top + 48) : 64;
        if (b.hp <= 6 && b.phase == 1) {
            saw_gate = true;
        }
        B10J_UpdateAndRender(&b, &f.context);
    }
    CHECK(saw_gate);
    CHECK(b.defeating);
    CHECK_EQ_INT(b.hp, 0);
    CHECK_EQ_INT(f.gameplay.score, 60000);
    CHECK_EQ_INT(f.fades, 1);
    const B10J frozen = b;
    const uint32_t rng = f.rng;
    CHECK(!B10J_UpdateAndRender(&b, &f.context));
    CHECK(memcmp(&b, &frozen, sizeof(b)) == 0);
    CHECK_EQ_INT(f.rng, rng);
    for (int32_t edge = 0; edge < 400 && !b.finished; edge++) {
        Frame_VSync();
        B10J_UpdateAndRender(&b, &f.context);
    }
    CHECK(b.finished);
    CHECK(f.cleared);
    CHECK(f.player.is_hit);
    CHECK(f.scene.active == &f.assets.next_scene);
    CHECK_EQ_INT(f.scene.active->id, 4);
    const B10J finished = b;
    CHECK(B10J_UpdateAndRender(&b, &f.context));
    CHECK(memcmp(&b, &finished, sizeof(b)) == 0);
    CHECK_EQ_INT(arena.used, used);
    Arena_Free(&arena);
}

TEST(two_squares_preserve_cloud_spawns_and_laser_slots)
{
    ARENA arena;
    Arena_Init(&arena, "b10j", 2 * 1024 * 1024);
    FIXTURE f;
    M_Battle(&f, &arena);
    Frame_Reset();
    B10J b;
    M_Setup(&b, &f, &arena);
    M_Enter(&b, &f);
    b.phase = 1;
    b.pattern = 2;
    b.frame = 191;
    b.squares[2] = (B10J_SQUARE) { .angle = 32, .radius = 112 };
    CHECK(B10J_UpdateAndRender(&b, &f.context));
    for (int32_t i = 0; i < 8; i++) {
        CHECK(f.pellets.slots[i].cloud_frame != 0);
    }
    CHECK(!f.pellets.spawn_with_cloud);
    b.phase = 3;
    b.pattern = 3;
    b.frame = 180;
    b.pattern_state = 50;
    b.laser_x[1] = 200;
    b.laser_y[1] = 128;
    CHECK(B10J_UpdateAndRender(&b, &f.context));
    CHECK(f.lasers.slots[1].alive);
    CHECK_EQ_INT(f.lasers.slots[1].origin_left, 200 * 256);
    const SHOOTOUT_LASER first = f.lasers.slots[1];
    b.frame = 184;
    b.laser_x[1] = 300;
    CHECK(B10J_UpdateAndRender(&b, &f.context));
    CHECK(memcmp(&first, &f.lasers.slots[1], sizeof(first)) == 0);
    Arena_Free(&arena);
}

TEST(hp_gate_waits_for_pattern_cleanup_and_invincibility)
{
    ARENA arena;
    Arena_Init(&arena, "b10j", 2 * 1024 * 1024);
    FIXTURE f;
    M_Battle(&f, &arena);
    Frame_Reset();
    B10J b;
    M_Setup(&b, &f, &arena);
    M_Enter(&b, &f);
    b.hp = 6;
    b.pattern = 0;
    b.frame = 360;
    b.hp_bar_filled = true;
    b.hit = (BOSS_HIT) { .frame = 0, .invincible = true };
    CHECK(B10J_UpdateAndRender(&b, &f.context));
    CHECK_EQ_INT(b.phase, 1);
    CHECK_EQ_INT(b.pattern, 99);
    CHECK_EQ_INT(b.frame, 0);
    for (int32_t i = 0; i < 39; i++) {
        CHECK(B10J_UpdateAndRender(&b, &f.context));
        CHECK_EQ_INT(b.phase, 1);
    }
    CHECK(B10J_UpdateAndRender(&b, &f.context));
    CHECK_EQ_INT(b.phase, 2);
    CHECK_EQ_INT(b.pattern, 0);
    CHECK_EQ_INT(b.hit.frame, 0);
    CHECK_EQ_INT(b.spread_interval, 4);
    CHECK_EQ_INT(b.spread_speed, 8);
    Arena_Free(&arena);
}

TEST(defeat_keeps_clouds_and_resets_only_five_lasers)
{
    ARENA arena;
    Arena_Init(&arena, "b10j", 2 * 1024 * 1024);
    FIXTURE f;
    M_Battle(&f, &arena);
    Frame_Reset();
    B10J b;
    M_Setup(&b, &f, &arena);
    M_Enter(&b, &f);
    b.phase = 3;
    b.pattern = 1;
    b.frame = 0;
    b.hp = 0;
    f.missiles.slots[0] = (MISSILE) { .flag = MISSILE_MOVING,
                                      .left = 200 * 16,
                                      .top = 200 * 16,
                                      .previous_left = -1,
                                      .previous_top = 88,
                                      .velocity = { 16, 0 } };
    f.pellets.slots[0] =
        (PELLET) { .moving = true, .left = 200 * 16, .top = 200 * 16 };
    f.pellets.slots[1].cloud_frame = 10;
    for (int32_t i = 0; i < 10; i++) {
        f.lasers.slots[i].alive = true;
    }
    CHECK(!B10J_UpdateAndRender(&b, &f.context));
    CHECK(b.defeating);
    CHECK_EQ_INT(f.fades, 1);
    CHECK_EQ_INT(f.missiles.slots[0].left, 201 * 16);
    CHECK_EQ_INT(f.missiles.slots[0].flag, MISSILE_FREE);
    CHECK(!f.pellets.slots[0].moving);
    CHECK_EQ_INT(f.pellets.slots[1].cloud_frame, 10);
    for (int32_t i = 0; i < 10; i++) {
        CHECK(f.lasers.slots[i].alive == (i >= 5));
    }
    CHECK(!f.cleared);
    CHECK(f.scene.active == nullptr);
    Arena_Free(&arena);
}

TEST(context_requires_storage_glyphs_and_audio_consumer)
{
    ARENA arena;
    Arena_Init(&arena, "b10j", 2 * 1024 * 1024);
    FIXTURE f;
    M_Battle(&f, &arena);
    f.context.fade_music = nullptr;
    CHECK(!Result_Ignore(B10J_ValidateContext(&f.context)));
    f.context.fade_music = M_Fade;
    f.hit_context.shots_output.action = nullptr;
    CHECK(!Result_Ignore(B10J_ValidateContext(&f.context)));
    f.hit_context.shots_output.action = M_Shot;
    f.health.backgrounds = nullptr;
    CHECK(!Result_Ignore(B10J_ValidateContext(&f.context)));
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
        Frame_Reset();
        B10J b;
        M_Setup(&b, &f, &arena);
        g_VSync.count1 = initial[run];
        g_VSync.count2 = 33;
        int32_t edges = 0;
        while (!B10J_UpdateAndRender(&b, &f.context) && edges < 110) {
            Frame_VSync();
            edges++;
        }
        CHECK_EQ_INT(edges, run == 0 ? 99 : 98);
        CHECK_EQ_INT(g_VSync.count1, 0);
        CHECK_EQ_INT(g_VSync.count2, 33 + edges);
        CHECK_EQ_INT(b.phase, 1);
        CHECK_EQ_INT(b.frame, 0);
        Arena_Free(&arena);
    }
}
