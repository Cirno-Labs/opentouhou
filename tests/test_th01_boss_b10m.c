#include "th01/boss_b10m.h"

#include "core/base/arena.h"

#include "test.h"

typedef struct {
    VIDEO video;
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
} FIXTURE;

static void M_Init(FIXTURE *const f, ARENA *const arena)
{
    *f = (FIXTURE) {};
    const ALLOCATOR *const a = Arena_Allocator(arena);
    Video_Init(&f->video, a);
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
    f->display.video = &f->video;
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

TEST(setup_shares_eye_assets_and_retains_static_history)
{
    ARENA arena;
    Arena_Init(&arena, "b10m", 2 * 1024 * 1024);
    FIXTURE f;
    M_Init(&f, &arena);
    B10M b = { .frame = 77,
               .phase = 13,
               .pattern_state = 17,
               .subphase = 8,
               .hit = { 39, true },
               .hp_bar_filled = true,
               .pentagram = { .angle = 450, .phase = 9 } };
    for (int32_t i = 0; i < 5; i++) {
        b.eyes[i].lock_frame = (int16_t)(i % 4);
        b.eyes[i].previous_left = (int16_t)(i * 11);
        f.missiles.slots[i].flag = MISSILE_MOVING;
        f.missiles.slots[i].previous_top = (int16_t)(i * 9);
    }
    memset(f.video.pages[0], 3, VIDEO_PAGE_SIZE);
    memset(f.video.pages[1], 5, VIDEO_PAGE_SIZE);
    const size_t used = arena.used;
    B10M_Setup(&b, &f.context);
    CHECK_EQ_INT(arena.used, used);
    CHECK_EQ_INT(b.hp, 16);
    CHECK_EQ_INT(b.phase, 0);
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
    CHECK_EQ_INT(f.video.pages[0][0], 3);
    CHECK_EQ_INT(f.video.pages[1][0], 5);
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
