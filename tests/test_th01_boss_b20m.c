#include "th01/boss_b20m.h"
#include "th01/session.h"
#include "core/base/arena.h"
#include "test.h"
static void M_Assets(B20M_ASSETS *const a, ARENA *const arena)
{
    *a = (B20M_ASSETS) {};
    uint8_t *const pixels = Arena_AllocZ(arena, VIDEO_PAGE_SIZE);
    uint8_t *const coverage = Arena_AllocZ(arena, VIDEO_PAGE_SIZE);
    a->shield = (BOS) { 48, 48, 4, pixels, coverage };
    a->wand = (BOS) { 96, 96, 2, pixels, coverage };
    a->dress = (BOS) { 96, 96, 4, pixels, coverage };
    a->vortex = (GRC) { 32, 32, 6, coverage };
    a->bird = (GRC) { 32, 32, 8, coverage };
    a->cross = (GRC) { 32, 32, 2, coverage };
    a->splash = (GRC) { 32, 32, 3, coverage };
    PI_IMAGE *const images[] = { &a->lower,          &a->upper,
                                 &a->backgrounds[0], &a->backgrounds[1],
                                 &a->backgrounds[2], &a->backgrounds[3],
                                 &a->backgrounds[4], &a->backgrounds[5] };
    for (int32_t i = 0; i < 8; i++) {
        *images[i] = (PI_IMAGE) {
            .width = 640, .height = 400, .has_palette = true, .pixels = pixels
        };
    }
    a->music.bytes = pixels;
    a->second_music.bytes = pixels;
    a->effects.bytes = pixels;
    a->star = pixels;
    a->flake = pixels;
    a->spark = pixels;
    a->leaf_left = pixels;
    a->leaf_right = pixels;
    a->interleave = Arena_AllocZ(arena, sizeof(INTERLEAVE_ASSETS));
}
TEST(setup_preserves_patterns_and_uses_grouped_wand_storage)
{
    ARENA arena;
    Arena_Init(&arena, "Sariel", 1024 * 1024);
    const ALLOCATOR *const allocator = Arena_Allocator(&arena);
    B20M_ASSETS a;
    M_Assets(&a, &arena);
    B20M b;
    B20M_Init(&b, allocator);
    CHECK(b.wand_background != nullptr);
    b.pattern_state = 12;
    b.leaves.left[4] = 123;
    b.birds.left[3] = 25.5;
    b.shield.lock_frame = 7;
    b.hit.frame = 17;
    b.hit.invincible = true;
    BOSS_SPRITES sprites = {};
    PALETTE_STATE palette = {};
    HUD_HP health = {};
    palette.saved.colors[6][0] = 9;
    BOSS_HIT_CONTEXT hit = { .palette = &palette, .health = &health };
    B20M_CONTEXT c = { .assets = &a, .sprites = &sprites, .hit = &hit };
    const size_t used = arena.used;
    B20M_Setup(&b, &c);
    CHECK_EQ_INT(arena.used, used);
    CHECK_EQ_INT(b.hp, 18);
    CHECK_EQ_INT(b.shield.left, 304);
    CHECK_EQ_INT(b.shield.top, 144);
    CHECK_EQ_INT(b.shield.orb_hitbox.left, 0);
    CHECK_EQ_INT(b.shield.orb_hitbox.right, 48);
    CHECK_EQ_INT(b.shield.lock_frame, 7);
    CHECK_EQ_INT(b.hit.frame, 17);
    CHECK(b.hit.invincible);
    CHECK_EQ_INT(b.pattern_state, 12);
    CHECK_EQ_INT(b.leaves.left[4], 123);
    CHECK(b.birds.left[3] == 25.5);
    CHECK_EQ_INT(b.wand.sprites.slot, 0);
    CHECK_EQ_INT(b.dress.sprites.slot, 1);
    CHECK_EQ_INT(b.wand.left, 296);
    CHECK_EQ_INT(b.dress.top, 192);
    CHECK_EQ_INT(health.first_white, 8);
    CHECK_EQ_INT(health.first_redwhite, 2);
    CHECK_EQ_INT(b.boss_palette.colors[6][0], 9);
    Arena_Free(&arena);
}
TEST(validation_requires_both_forms_and_all_cached_resources)
{
    ARENA arena;
    Arena_Init(&arena, "Sariel validation", 1024 * 1024);
    B20M_ASSETS a;
    M_Assets(&a, &arena);
    CHECK(Result_Ignore(B20M_ValidateAssets(&a)));
    a.backgrounds[5].pixels = nullptr;
    CHECK(!Result_Ignore(B20M_ValidateAssets(&a)));
    a.backgrounds[5].pixels = a.lower.pixels;
    a.second_music.bytes = nullptr;
    CHECK(!Result_Ignore(B20M_ValidateAssets(&a)));
    a.second_music.bytes = a.music.bytes;
    a.bird.count = 7;
    CHECK(!Result_Ignore(B20M_ValidateAssets(&a)));
    Arena_Free(&arena);
}

#include <string.h>
typedef struct {
    B20M_ASSETS assets;
    B20M boss;
    BOSS_SPRITES sprites;
    PALETTE_STATE palette;
    HUD_HP health;
    HUD hud;
    HUD_CONTEXT display;
    BOSS_HIT_CONTEXT hit;
    B20M_CONTEXT context;
    PELLETS pellets;
    PELLET_SHARED shared;
    SHOOTOUT_LASERS lasers;
    STAGE_OBJECTS objects;
    PLAYER player;
    ORB orb;
    SHOTS shots;
    STAGE_GAMEPLAY gameplay;
    GAME_TEXT_FONT font;
    PTN stage;
    uint32_t high_score, rng;
    int16_t lives;
    bool damage, cleared, first_stage, timer_initialized;
    GRF mask;
    int32_t effects[17], audio_count;
    B20M_AUDIO_ACTION audio[8];
} FIXTURE;

static void M_Effect(void *const user, const int16_t effect)
{
    FIXTURE *const f = user;
    CHECK(effect > 0 && effect <= 16);
    f->effects[effect]++;
}
static void M_Audio(
    void *const user, const B20M_AUDIO_ACTION action,
    const B20M_ASSETS *const assets)
{
    FIXTURE *const f = user;
    CHECK(assets == &f->assets);
    CHECK(f->audio_count < 8);
    f->audio[f->audio_count++] = action;
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
static void M_Fixture(FIXTURE *const f, ARENA *const arena)
{
    *f = (FIXTURE) {};
    GameInput_Init();
    StageTimer_Init();
    M_Assets(&f->assets, arena);
    const ALLOCATOR *const a = Arena_Allocator(arena);
    Video_Init(a);
    Tram_Init(a);
    Palette_Init(&f->palette);
    Hud_Init(&f->hud, a);
    HudHp_Init(&f->health, a);
    GAME_TEXT_GLYPH glyphs[105] = {};
    for (uint32_t i = 0; i < 10; i++) {
        glyphs[i].codepoint = 0xff10 + i;
        glyphs[i].width = 16;
        for (int32_t y = 0; y < 16; y++) {
            glyphs[i].rows[y] = 0x8000u >> ((i + y) % 16);
        }
    }
    for (uint32_t i = 0; i < 95; i++) {
        glyphs[i + 10].codepoint = 32 + i;
        glyphs[i + 10].width = 8;
        for (int32_t y = 0; y < 16; y++) {
            glyphs[i + 10].rows[y] = 0xaa00;
        }
    }
    CHECK(Result_Ignore(GameText_LoadGlyphs(glyphs, 105, a, &f->font)));
    f->stage = (PTN) { .count = 4, .pixels = Allocator_Alloc(a, 4096) };
    memset(f->stage.pixels, PTN_TRANSPARENT, 4096);
    f->mask.coverage = Allocator_AllocZ(a, 640 * 66);
    f->player.left = 300;
    f->player.invincible = true;
    f->lives = 3;
    f->orb.top = 64;
    f->orb.previous_top = 64;
    f->display = (HUD_CONTEXT) { .mask = &f->mask,
                                 .stage_number = 20,
                                 .sprites = &f->stage,
                                 .font = &f->font,
                                 .gameplay = &f->gameplay,
                                 .player = &f->player,
                                 .high_score = &f->high_score,
                                 .remaining_lives = &f->lives };
    f->hit = (BOSS_HIT_CONTEXT) { .palette = &f->palette,
                                  .hud = &f->hud,
                                  .display = &f->display,
                                  .health = &f->health,
                                  .shots = &f->shots,
                                  .shots_output = { M_Shot, f },
                                  .orb = &f->orb,
                                  .test_damage = &f->damage,
                                  .frame_rand = 0x12345678,
                                  .effect = M_Effect,
                                  .user = f };
    f->context = (B20M_CONTEXT) { .assets = &f->assets,
                                  .sprites = &f->sprites,
                                  .hit = &f->hit,
                                  .pellets = &f->pellets,
                                  .pellet_shared = &f->shared,
                                  .lasers = &f->lasers,
                                  .objects = &f->objects,
                                  .rng = &f->rng,
                                  .game_cleared = &f->cleared,
                                  .audio = M_Audio,
                                  .first_stage = &f->first_stage,
                                  .timer_initialized = &f->timer_initialized };
    B20M_Init(&f->boss, a);
    B20M_Setup(&f->boss, &f->context);
    g_Session.settings.difficulty = f->display.rank;
}

static bool M_Tick(FIXTURE *const f, uint64_t *const tick)
{
    const bool done = B20M_UpdateAndRender(&f->boss, &f->context, *tick);
    (*tick)++;
    Frame_VSync();
    return done;
}
TEST(cached_scroll_entrance_and_diagonal_wait_without_allocating)
{
    ARENA arena;
    Arena_Init(&arena, "Sariel entrance", 4 * 1024 * 1024);
    FIXTURE f;
    M_Fixture(&f, &arena);
    memset(f.assets.lower.pixels, 12, VIDEO_PAGE_SIZE);
    f.rng = 5;
    Frame_Reset();
    uint64_t tick = 0;
    const size_t used = arena.used;
    B20M_BeginEntrance(&f.boss, &f.context);
    CHECK(f.pellets.interlace);
    CHECK_EQ_INT(f.audio_count, 3);
    CHECK(!B20M_ResumeEntrance(&f.boss, &f.context, tick));
    const B20M pending = f.boss;
    CHECK(!B20M_ResumeEntrance(&f.boss, &f.context, tick));
    CHECK(memcmp(&pending, &f.boss, sizeof(pending)) == 0);
    int32_t polls = 0;
    while (f.boss.entrance_active && polls++ < 1500) {
        tick++;
        Frame_VSync();
        B20M_ResumeEntrance(&f.boss, &f.context, tick);
    }
    CHECK(!f.boss.entrance_active);
    CHECK_EQ_INT(f.rng, f.hit.frame_rand);
    CHECK_EQ_INT(g_Video.drawing_page, 0);
    CHECK_EQ_INT(g_Video.scroll_top, 0);
    CHECK_EQ_INT(arena.used, used);
    CHECK_EQ_INT(f.audio_count, 3);
    Arena_Free(&arena);
}
TEST(ring_entrance_preserves_pending_polls_and_reserves_wand_background)
{
    ARENA arena;
    Arena_Init(&arena, "Sariel rings", 4 * 1024 * 1024);
    FIXTURE f;
    M_Fixture(&f, &arena);
    Frame_Reset();
    uint64_t tick = 0;
    const size_t used = arena.used;
    CHECK(!B20M_UpdateAndRender(&f.boss, &f.context, tick));
    const B20M pending = f.boss;
    const uint32_t rng = f.rng;
    CHECK(!B20M_UpdateAndRender(&f.boss, &f.context, tick));
    CHECK(memcmp(&pending, &f.boss, sizeof(pending)) == 0);
    CHECK_EQ_INT(f.rng, rng);
    for (int32_t i = 0; f.boss.sequence != 0 && i < 200; i++) {
        tick++;
        Frame_VSync();
        B20M_UpdateAndRender(&f.boss, &f.context, tick);
    }
    CHECK_EQ_INT(f.boss.phase, 1);
    CHECK_EQ_INT(f.boss.frame, 0);
    CHECK_EQ_INT(f.boss.entrance_radius, 352);
    CHECK_EQ_INT(f.boss.shield.left, 304);
    CHECK(f.boss.patterns_until_next >= 1 && f.boss.patterns_until_next <= 6);
    CHECK_EQ_INT(arena.used, used);
    Arena_Free(&arena);
}
TEST(all_first_form_patterns_finish_with_real_projectile_pools)
{
    const int8_t phases[] = { 1, 3, 5, 7 };
    const int16_t counts[] = { 3, 2, 6, 3 };
    for (int32_t rank = 0; rank < 4; rank++) {
        for (int32_t phase = 0; phase < 4; phase++) {
            for (int16_t pattern = 0; pattern < counts[phase]; pattern++) {
                ARENA arena;
                Arena_Init(&arena, "Sariel first form", 4 * 1024 * 1024);
                FIXTURE f;
                M_Fixture(&f, &arena);
                f.display.rank = rank;
                g_Session.settings.difficulty = rank;
                f.rng = 0x12345678u + (uint32_t)rank;
                f.boss.phase = phases[phase];
                f.boss.pattern = pattern;
                f.boss.patterns_until_next = 30000;
                Frame_Reset();
                uint64_t tick = 0;
                const size_t used = arena.used;
                int32_t calls = 0;
                while (f.boss.pattern == pattern && calls++ < 2000) {
                    M_Tick(&f, &tick);
                }
                CHECK(calls < 2000);
                CHECK_EQ_INT(f.boss.hp, 18);
                CHECK_EQ_INT(f.boss.phase, phases[phase]);
                CHECK_EQ_INT(arena.used, used);
                Arena_Free(&arena);
            }
        }
    }
}
TEST(all_second_form_patterns_finish_with_persistent_leaf_state)
{
    for (int32_t rank = 0; rank < 4; rank++) {
        for (int16_t pattern = 0; pattern < 9; pattern++) {
            ARENA arena;
            Arena_Init(&arena, "Sariel second form", 4 * 1024 * 1024);
            FIXTURE f;
            M_Fixture(&f, &arena);
            f.display.rank = rank;
            g_Session.settings.difficulty = rank;
            f.rng = 0xabcdef01u + (uint32_t)rank;
            f.boss.phase = 100;
            f.boss.hp = 6;
            f.boss.pattern = pattern;
            f.boss.pattern_state = 24;
            Frame_Reset();
            uint64_t tick = 0;
            const size_t used = arena.used;
            int32_t calls = 0;
            while (f.boss.pattern == pattern && calls++ < 1000) {
                M_Tick(&f, &tick);
            }
            CHECK(calls < 1000);
            CHECK_EQ_INT(f.boss.hp, 6);
            CHECK_EQ_INT(f.boss.phase, 100);
            CHECK_EQ_INT(arena.used, used);
            Arena_Free(&arena);
        }
    }
}
TEST(form_change_resets_player_and_flight_but_retains_previous_orb)
{
    ARENA arena;
    Arena_Init(&arena, "Sariel transform", 4 * 1024 * 1024);
    FIXTURE f;
    M_Fixture(&f, &arena);
    f.boss.phase = 99;
    f.boss.hp = 0;
    f.orb.previous_left = 123;
    f.orb.previous_top = 234;
    f.player.invincible = true;
    f.player.invincibility_time = 91;
    f.player.deflecting = true;
    f.player.bomb_damaging = true;
    f.boss.leaves.flag[7] = 6;
    g_GameInput.bomb = true;
    Frame_Reset();
    uint64_t tick = 0;
    const size_t used = arena.used;
    CHECK(!B20M_UpdateAndRender(&f.boss, &f.context, tick));
    CHECK_EQ_INT(f.player.left, 304);
    CHECK_EQ_INT(f.orb.left, 600);
    CHECK_EQ_INT(f.orb.top, 280);
    CHECK_EQ_INT(f.orb.previous_left, 123);
    CHECK_EQ_INT(f.orb.previous_top, 234);
    CHECK(!f.player.deflecting);
    CHECK(!f.player.bomb_damaging);
    CHECK(!g_GameInput.bomb);
    for (int32_t i = 0; f.boss.sequence != 0 && i < 1000; i++) {
        tick++;
        Frame_VSync();
        B20M_UpdateAndRender(&f.boss, &f.context, tick);
    }
    CHECK_EQ_INT(f.boss.phase, 100);
    CHECK_EQ_INT(f.boss.hp, 6);
    CHECK_EQ_INT(f.boss.frame, 0);
    CHECK_EQ_INT(f.player.invincibility_time, 0);
    CHECK(!f.player.invincible);
    CHECK_EQ_INT(f.boss.leaves.flag[7], 0);
    CHECK_EQ_INT(f.health.first_white, 10);
    CHECK_EQ_INT(f.health.first_redwhite, 3);
    CHECK(f.first_stage);
    CHECK_EQ_INT(f.audio_count, 3);
    CHECK_EQ_INT(f.audio[0], B20M_AUDIO_FADE);
    CHECK_EQ_INT(f.audio[1], B20M_AUDIO_LOAD_SECOND_MUSIC);
    CHECK_EQ_INT(f.audio[2], B20M_AUDIO_PLAY);
    CHECK_EQ_INT(arena.used, used);
    Arena_Free(&arena);
}
TEST(final_current_orb_hit_finishes_custom_victory_and_becomes_inert)
{
    ARENA arena;
    Arena_Init(&arena, "Sariel victory", 4 * 1024 * 1024);
    FIXTURE f;
    M_Fixture(&f, &arena);
    f.boss.phase = 100;
    f.boss.hp = 1;
    f.orb.left = 320;
    f.orb.top = 120;
    Frame_Reset();
    uint64_t tick = 0;
    const size_t used = arena.used;
    CHECK(B20M_UpdateAndRender(&f.boss, &f.context, tick));
    CHECK_EQ_INT(f.boss.phase, 101);
    CHECK_EQ_INT(f.boss.hp, 0);
    CHECK_EQ_INT(f.gameplay.score, 10000);
    CHECK_EQ_INT(f.audio[0], B20M_AUDIO_STOP);
    for (int32_t i = 0; !f.cleared && i < 1500; i++) {
        M_Tick(&f, &tick);
    }
    CHECK(f.cleared);
    CHECK(f.boss.finished);
    CHECK_EQ_INT(f.gameplay.score, 60000);
    CHECK_EQ_INT(f.boss.frame, 231);
    CHECK_EQ_INT(g_Video.scroll_top, 0);
    CHECK_EQ_INT(f.audio_count, 2);
    CHECK_EQ_INT(f.audio[1], B20M_AUDIO_FADE);
    CHECK_EQ_INT(arena.used, used);
    const B20M done = f.boss;
    CHECK(B20M_UpdateAndRender(&f.boss, &f.context, tick));
    CHECK(memcmp(&done, &f.boss, sizeof(done)) == 0);
    CHECK_EQ_INT(f.gameplay.score, 60000);
    Arena_Free(&arena);
}
TEST(bird_collision_waits_before_pattern_rng_and_hit_tail)
{
    ARENA arena;
    Arena_Init(&arena, "Sariel bird safety", 4 * 1024 * 1024);
    FIXTURE f;
    M_Fixture(&f, &arena);
    f.boss.phase = 1;
    f.boss.frame = 49;
    f.boss.pattern = 0;
    f.boss.patterns_until_next = 10;
    f.player.invincible = false;
    f.boss.birds.alive[0] = true;
    f.boss.birds.left[0] = 300;
    f.boss.birds.top[0] = 368;
    Frame_Reset();
    uint64_t tick = 100;
    const uint32_t rng = f.rng;
    const size_t used = arena.used;
    CHECK(!B20M_UpdateAndRender(&f.boss, &f.context, tick));
    CHECK(f.player.is_hit);
    CHECK_EQ_INT(f.rng, rng);
    const B20M pending = f.boss;
    CHECK(!B20M_UpdateAndRender(&f.boss, &f.context, tick + 5));
    CHECK(memcmp(&pending, &f.boss, sizeof(pending)) == 0);
    CHECK(B20M_UpdateAndRender(&f.boss, &f.context, tick + 6));
    CHECK(f.rng != rng);
    CHECK_EQ_INT(f.boss.frame, 50);
    CHECK_EQ_INT(f.boss.resume_stage, 0);
    CHECK_EQ_INT(arena.used, used);
    Arena_Free(&arena);
}