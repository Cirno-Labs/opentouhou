#include "th01/boss_b10j.h"
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
    PALETTE_STATE palette;
    PALETTE stage_palette;
    B10J_ASSETS assets;
    HUD_CONTEXT display;
    BOSS_HIT_CONTEXT hit_context;
    B10J_CONTEXT context;
    GAME_TEXT_FONT font;
    PTN stage, miko;
    uint32_t high_score;
    int16_t lives;
    bool test_damage, cleared;
    int32_t effects[17], fades;
} FIXTURE;

static void M_Init(FIXTURE *const f, ARENA *const arena)
{
    *f = (FIXTURE) {};
    Th01Test_ResetWorld();
    const ALLOCATOR *const a = Arena_Allocator(arena);
    Video_Init(a);
    Palette_Init(&f->palette);
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
    f->hit_context =
        (BOSS_HIT_CONTEXT) { .palette = &f->palette, .display = &f->display };
    f->context = (B10J_CONTEXT) { .assets = &f->assets,
                                  .hit = &f->hit_context,
                                  .stage_palette = &f->stage_palette };
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
    f->lives = 3;
    g_Rng = 0xb10b0000;
    g_Player.invincible = true;
    g_Player.left = 300;
    g_Orb.top = 64;
    g_Orb.previous_top = 64;
    f->display = (HUD_CONTEXT) { .sprites = &f->stage,
                                 .font = &f->font,
                                 .high_score = &f->high_score,
                                 .remaining_lives = &f->lives };
    f->hit_context.shots_output = (SHOTS_OUTPUT) { M_Shot, f };
    f->hit_context.test_damage = &f->test_damage;
    f->hit_context.effect = M_Effect;
    f->hit_context.user = f;
    f->context.miko = &f->miko;
    f->context.stage_cleared = &f->cleared;
    f->context.fade_music = M_Fade;
    g_Session.settings.difficulty = f->display.rank;
}

TEST(setup_preserves_saved_palette_and_all_sixteen_waits)
{
    ARENA arena;
    Arena_Init(&arena, "b10j", 2 * 1024 * 1024);
    FIXTURE f;
    M_Init(&f, &arena);
    B10J_Init();
    g_B10J.frame = 91;
    g_B10J.hp = 3;
    g_B10J.still.lock_frame = 2;
    g_B10J.still.previous_top = 27;
    g_B10J.hit.frame = 17;
    g_B10J.pattern_state = 99;
    g_Missiles.slots[0].flag = MISSILE_MOVING;
    g_Missiles.slots[0].previous_top = 36;
    Frame_Reset();
    g_VSync.count1 = 12;
    g_VSync.count2 = 19;
    B10J_BeginSetup(&f.context);
    const size_t used = arena.used;
    CHECK(!B10J_ResumeSetup(&f.context));
    CHECK_EQ_INT(g_B10J.frame, 91);
    CHECK_EQ_INT(g_B10J.hp, 3);
    for (int32_t step = 0; step < 16; step++) {
        Frame_VSync();
        if (step == 15) {
            CHECK(B10J_ResumeSetup(&f.context));
        } else {
            CHECK(!B10J_ResumeSetup(&f.context));
            const B10J frozen = g_B10J;
            CHECK(!B10J_ResumeSetup(&f.context));
            CHECK(memcmp(&g_B10J, &frozen, sizeof(g_B10J)) == 0);
            CHECK_EQ_INT(g_B10J.hp, 3);
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
    CHECK_EQ_INT(g_B10J.still.left, 256);
    CHECK_EQ_INT(g_B10J.still.top, 64);
    CHECK_EQ_INT(g_B10J.still.previous_top, 27);
    CHECK_EQ_INT(g_B10J.still.lock_frame, 2);
    CHECK_EQ_INT(g_B10J.hit.frame, 17);
    CHECK_EQ_INT(g_B10J.pattern_state, 99);
    CHECK_EQ_INT(g_B10J.hp, 12);
    CHECK_EQ_INT(g_B10J.frame, 0);
    CHECK_EQ_INT(g_B10J.phase, B10J_PHASE_START);
    CHECK_EQ_INT(g_HudHp.first_white, 6);
    CHECK_EQ_INT(g_HudHp.first_redwhite, 2);
    CHECK_EQ_INT(g_Missiles.slots[0].flag, MISSILE_FREE);
    CHECK_EQ_INT(g_Missiles.slots[0].previous_top, 36);
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
    M_Init(&f, &arena);
    CHECK(Result_Ignore(B10J_ValidateAssets(&f.assets)));
    f.assets.animation.count = 4;
    CHECK(!Result_Ignore(B10J_ValidateAssets(&f.assets)));
    f.assets.animation.count = 5;
    f.assets.next_scene.id = 3;
    CHECK(!Result_Ignore(B10J_ValidateAssets(&f.assets)));
    Arena_Free(&arena);
}

static void M_Setup(FIXTURE *const f)
{
    B10J_Init();
    B10J_BeginSetup(&f->context);
    while (!B10J_ResumeSetup(&f->context)) {
        Frame_VSync();
    }
}

static void M_Enter(FIXTURE *const f)
{
    for (int32_t edge = 0; edge < 110; edge++) {
        if (B10J_UpdateAndRender(&f->context)) {
            CHECK_EQ_INT(g_B10J.phase, B10J_PHASE_FIRST);
            CHECK_EQ_INT(g_B10J.frame, 0);
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
    M_Setup(&f);
    g_VSync.count1 = 0;
    g_Missiles.slots[0] = (MISSILE) { .flag = MISSILE_MOVING,
                                      .left = 200 * 16,
                                      .top = 200 * 16,
                                      .previous_left = -1,
                                      .velocity = { 16, 0 } };
    const size_t used = arena.used;
    CHECK(!B10J_UpdateAndRender(&f.context));
    CHECK_EQ_INT(g_Missiles.slots[0].left, 201 * 16);
    CHECK_EQ_INT(g_B10J.frame, 1);
    const B10J frozen = g_B10J;
    const uint32_t frozen_rng = g_Rng;
    CHECK(!B10J_UpdateAndRender(&f.context));
    CHECK(memcmp(&g_B10J, &frozen, sizeof(g_B10J)) == 0);
    CHECK_EQ_INT(g_Rng, frozen_rng);
    for (int32_t edge = 1; edge <= 100; edge++) {
        Frame_VSync();
        const bool done = B10J_UpdateAndRender(&f.context);
        CHECK(done == (edge == 100));
    }
    CHECK_EQ_INT(g_B10J.phase, B10J_PHASE_FIRST);
    CHECK_EQ_INT(g_B10J.frame, 0);
    CHECK_EQ_INT(g_B10J.still.left, 256);
    CHECK_EQ_INT(g_B10J.still.top, 120);
    CHECK_EQ_INT(g_Missiles.slots[0].left, 201 * 16);
    uint32_t expected = frozen_rng;
    MasterRng_Next(&expected);
    CHECK_EQ_INT(g_Rng, expected);
    CHECK_EQ_INT(g_B10J.hop, 255);
    CHECK_EQ_INT(g_B10J.spread_interval, 4);
    CHECK_EQ_INT(g_B10J.spread_speed, 16);
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
        g_Session.settings.difficulty = rank;
        Frame_Reset();
        M_Setup(&f);
        M_Enter(&f);
        const size_t used = arena.used;
        uint16_t patterns = 0;
        int32_t loops = 0;
        for (int32_t step = 0; step < 12000 && loops < 2; step++) {
            const int8_t phase = g_B10J.phase;
            const int16_t pattern = g_B10J.pattern;
            if ((phase == B10J_PHASE_FIRST || phase == B10J_PHASE_SECOND)
                && pattern >= 0 && pattern < 4) {
                patterns |=
                    (uint16_t)(1u
                               << (pattern
                                   + (phase == B10J_PHASE_SECOND ? 4 : 0)));
            }
            if ((patterns & 15) == 15) {
                g_B10J.hp = 6;
            }
            f.hit_context.frame_rand++;
            CHECK(B10J_UpdateAndRender(&f.context));
            if (phase == B10J_PHASE_SECOND && pattern == 3
                && g_B10J.pattern == 0) {
                loops++;
            }
        }
        CHECK_EQ_INT(patterns, 255);
        CHECK_EQ_INT(loops, 2);
        CHECK_EQ_INT(g_B10J.hp, 6);
        CHECK_EQ_INT(f.fades, 0);
        CHECK(!g_B10J.finished);
        CHECK(g_Scene.active == nullptr);
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
    M_Setup(&f);
    M_Enter(&f);
    const size_t used = arena.used;
    bool saw_gate = false;
    for (int32_t step = 0; step < 5000 && !g_B10J.defeating; step++) {
        f.hit_context.frame_rand++;
        const bool colliding = step % 50 == 20
            && (g_B10J.phase == B10J_PHASE_SECOND
                || (g_B10J.phase == B10J_PHASE_FIRST && g_B10J.hp > 6));
        g_Orb.previous_left =
            colliding ? Math16_Wrap(g_B10J.still.left + 48) : 0;
        g_Orb.previous_top =
            colliding ? Math16_Wrap(g_B10J.still.top + 48) : 64;
        if (g_B10J.hp <= 6 && g_B10J.phase == B10J_PHASE_FIRST) {
            saw_gate = true;
        }
        B10J_UpdateAndRender(&f.context);
    }
    CHECK(saw_gate);
    CHECK(g_B10J.defeating);
    CHECK_EQ_INT(g_B10J.hp, 0);
    CHECK_EQ_INT(g_Stage.score, 60000);
    CHECK_EQ_INT(f.fades, 1);
    const B10J frozen = g_B10J;
    const uint32_t rng = g_Rng;
    CHECK(!B10J_UpdateAndRender(&f.context));
    CHECK(memcmp(&g_B10J, &frozen, sizeof(g_B10J)) == 0);
    CHECK_EQ_INT(g_Rng, rng);
    for (int32_t edge = 0; edge < 400 && !g_B10J.finished; edge++) {
        Frame_VSync();
        B10J_UpdateAndRender(&f.context);
    }
    CHECK(g_B10J.finished);
    CHECK(f.cleared);
    CHECK(g_Player.is_hit);
    CHECK(g_Scene.active == &f.assets.next_scene);
    CHECK_EQ_INT(g_Scene.active->id, 4);
    const B10J finished = g_B10J;
    CHECK(B10J_UpdateAndRender(&f.context));
    CHECK(memcmp(&g_B10J, &finished, sizeof(g_B10J)) == 0);
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
    M_Setup(&f);
    M_Enter(&f);
    g_B10J.phase = B10J_PHASE_FIRST;
    g_B10J.pattern = 2;
    g_B10J.frame = 191;
    g_B10J.squares[2] = (B10J_SQUARE) { .angle = 32, .radius = 112 };
    CHECK(B10J_UpdateAndRender(&f.context));
    for (int32_t i = 0; i < 8; i++) {
        CHECK(g_Pellets.slots[i].cloud_frame != 0);
    }
    CHECK(!g_Pellets.spawn_with_cloud);
    g_B10J.phase = B10J_PHASE_SECOND;
    g_B10J.pattern = 3;
    g_B10J.frame = 180;
    g_B10J.pattern_state = 50;
    g_B10J.laser_x[1] = 200;
    g_B10J.laser_y[1] = 128;
    CHECK(B10J_UpdateAndRender(&f.context));
    CHECK(g_ShootoutLasers.slots[1].alive);
    CHECK_EQ_INT(g_ShootoutLasers.slots[1].origin_left, 200 * 256);
    const SHOOTOUT_LASER first = g_ShootoutLasers.slots[1];
    g_B10J.frame = 184;
    g_B10J.laser_x[1] = 300;
    CHECK(B10J_UpdateAndRender(&f.context));
    CHECK(memcmp(&first, &g_ShootoutLasers.slots[1], sizeof(first)) == 0);
    Arena_Free(&arena);
}

TEST(hp_gate_waits_for_pattern_cleanup_and_invincibility)
{
    ARENA arena;
    Arena_Init(&arena, "b10j", 2 * 1024 * 1024);
    FIXTURE f;
    M_Battle(&f, &arena);
    Frame_Reset();
    M_Setup(&f);
    M_Enter(&f);
    g_B10J.hp = 6;
    g_B10J.pattern = 0;
    g_B10J.frame = 360;
    g_B10J.hp_bar_filled = true;
    g_B10J.hit = (BOSS_HIT) { .frame = 0, .invincible = true };
    CHECK(B10J_UpdateAndRender(&f.context));
    CHECK_EQ_INT(g_B10J.phase, B10J_PHASE_FIRST);
    CHECK_EQ_INT(g_B10J.pattern, 99);
    CHECK_EQ_INT(g_B10J.frame, 0);
    for (int32_t i = 0; i < 39; i++) {
        CHECK(B10J_UpdateAndRender(&f.context));
        CHECK_EQ_INT(g_B10J.phase, B10J_PHASE_FIRST);
    }
    CHECK(B10J_UpdateAndRender(&f.context));
    CHECK_EQ_INT(g_B10J.phase, B10J_PHASE_SPREAD);
    CHECK_EQ_INT(g_B10J.pattern, 0);
    CHECK_EQ_INT(g_B10J.hit.frame, 0);
    CHECK_EQ_INT(g_B10J.spread_interval, 4);
    CHECK_EQ_INT(g_B10J.spread_speed, 8);
    Arena_Free(&arena);
}

TEST(defeat_keeps_clouds_and_resets_only_five_lasers)
{
    ARENA arena;
    Arena_Init(&arena, "b10j", 2 * 1024 * 1024);
    FIXTURE f;
    M_Battle(&f, &arena);
    Frame_Reset();
    M_Setup(&f);
    M_Enter(&f);
    g_B10J.phase = B10J_PHASE_SECOND;
    g_B10J.pattern = 1;
    g_B10J.frame = 0;
    g_B10J.hp = 0;
    g_Missiles.slots[0] = (MISSILE) { .flag = MISSILE_MOVING,
                                      .left = 200 * 16,
                                      .top = 200 * 16,
                                      .previous_left = -1,
                                      .previous_top = 88,
                                      .velocity = { 16, 0 } };
    g_Pellets.slots[0] =
        (PELLET) { .moving = true, .left = 200 * 16, .top = 200 * 16 };
    g_Pellets.slots[1].cloud_frame = 10;
    for (int32_t i = 0; i < 10; i++) {
        g_ShootoutLasers.slots[i].alive = true;
    }
    CHECK(!B10J_UpdateAndRender(&f.context));
    CHECK(g_B10J.defeating);
    CHECK_EQ_INT(f.fades, 1);
    CHECK_EQ_INT(g_Missiles.slots[0].left, 201 * 16);
    CHECK_EQ_INT(g_Missiles.slots[0].flag, MISSILE_FREE);
    CHECK(!g_Pellets.slots[0].moving);
    CHECK_EQ_INT(g_Pellets.slots[1].cloud_frame, 10);
    for (int32_t i = 0; i < 10; i++) {
        CHECK(g_ShootoutLasers.slots[i].alive == (i >= 5));
    }
    CHECK(!f.cleared);
    CHECK(g_Scene.active == nullptr);
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
        M_Setup(&f);
        g_VSync.count1 = initial[run];
        g_VSync.count2 = 33;
        int32_t edges = 0;
        while (!B10J_UpdateAndRender(&f.context) && edges < 110) {
            Frame_VSync();
            edges++;
        }
        CHECK_EQ_INT(edges, run == 0 ? 99 : 98);
        CHECK_EQ_INT(g_VSync.count1, 0);
        CHECK_EQ_INT(g_VSync.count2, 33 + edges);
        CHECK_EQ_INT(g_B10J.phase, B10J_PHASE_FIRST);
        CHECK_EQ_INT(g_B10J.frame, 0);
        Arena_Free(&arena);
    }
}
