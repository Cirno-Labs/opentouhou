#include "th01/boss_b15j.h"

#include "core/base/arena.h"

#include "test.h"

typedef struct {
    VIDEO video;
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
    Video_Init(&f->video, a);
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
    f->display.video = &f->video;
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
