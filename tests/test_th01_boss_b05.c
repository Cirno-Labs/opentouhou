#include "th01/boss_b05.h"

#include "core/base/arena.h"

#include "test.h"

typedef struct {
    VIDEO video;
    PALETTE_STATE palette;
    PALETTE stage_palette;
    B05_ASSETS assets;
    BOSS_SPRITES bank;
    HUD_HP health;
    PARTICLES particles;
    HUD_CONTEXT display;
    BOSS_HIT_CONTEXT hit_context;
    B05_CONTEXT context;
} FIXTURE;

static void M_Init(FIXTURE *const f, ARENA *const arena)
{
    *f = (FIXTURE) {};
    const ALLOCATOR *const allocator = Arena_Allocator(arena);
    Video_Init(&f->video, allocator);
    Palette_Init(&f->palette);
    const uint8_t *const pixels = Arena_AllocZ(arena, 96 * 96 * 8);
    const uint8_t *const coverage = Arena_AllocZ(arena, 96 * 96 * 8);
    f->assets.sphere = (BOS) { 96, 96, 8, pixels, coverage };
    f->assets.flash = (BOS) { 96, 96, 3, pixels, coverage };
    f->assets.person = (BOS) { 96, 96, 5, pixels, coverage };
    f->assets.background = (PI_IMAGE) {
        .width = 640,
        .height = 400,
        .has_palette = true,
        .pixels = Arena_AllocZ(arena, VIDEO_PAGE_SIZE),
    };
    for (int32_t color = 0; color < 16; color++) {
        for (int32_t component = 0; component < 3; component++) {
            f->assets.palette.colors[color][component] =
                (int8_t)((color + component * 3) % 16);
        }
    }
    f->display.video = &f->video;
    f->hit_context = (BOSS_HIT_CONTEXT) {
        .palette = &f->palette,
        .display = &f->display,
        .health = &f->health,
    };
    f->context = (B05_CONTEXT) {
        .assets = &f->assets,
        .sprites = &f->bank,
        .hit = &f->hit_context,
        .stage_palette = &f->stage_palette,
        .particles = &f->particles,
    };
}

TEST(setup_binds_real_banks_and_preserves_static_history)
{
    ARENA arena;
    Arena_Init(&arena, "b05", 2 * 1024 * 1024);
    FIXTURE f;
    M_Init(&f, &arena);
    memset(f.video.pages[0], 3, VIDEO_PAGE_SIZE);
    memset(f.video.pages[1], 5, VIDEO_PAGE_SIZE);
    const size_t used = arena.used;
    B05 boss = {
        .frame = 77,
        .pattern = 3,
        .velocity_x = 999,
        .velocity_y = -4,
        .angle = 42,
        .direction = -1,
        .hp_bar_filled = true,
        .hit = { .frame = 23, .invincible = true },
        .sphere = { .previous_left = 17, .lock_frame = 2 },
        .person = { .image = 5 },
    };
    f.particles.cycle = 73;
    f.particles.velocity_base[0] = 9;
    f.particles.alive[0] = 255;
    B05_Setup(&boss, &f.context);
    CHECK_EQ_INT(arena.used, used);
    CHECK(f.bank.entities[0] == &f.assets.sphere);
    CHECK(f.bank.entities[1] == &f.assets.flash);
    CHECK(f.bank.entities[2] == &f.assets.person);
    CHECK_EQ_INT(boss.sphere.left, 640);
    CHECK_EQ_INT(boss.sphere.top, 64);
    CHECK_EQ_INT(boss.sphere.orb_hitbox.left, 24);
    CHECK_EQ_INT(boss.sphere.orb_hitbox.right, 72);
    CHECK_EQ_INT(boss.sphere.move_clamp.right, 736);
    CHECK_EQ_INT(boss.sphere.image, 0);
    CHECK_EQ_INT(boss.sphere.previous_left, 17);
    CHECK_EQ_INT(boss.sphere.lock_frame, 2);
    CHECK_EQ_INT(boss.person.image, 5);
    CHECK_EQ_INT(boss.phase, 0);
    CHECK_EQ_INT(boss.hp, 8);
    CHECK_EQ_INT(boss.frame, 77);
    CHECK_EQ_INT(boss.hit.frame, 23);
    CHECK(boss.hit.invincible && boss.hp_bar_filled);
    CHECK_EQ_INT(boss.pattern, 3);
    CHECK_EQ_INT(boss.velocity_x, 999);
    CHECK_EQ_INT(boss.direction, -1);
    CHECK_EQ_INT(f.health.first_white, 6);
    CHECK_EQ_INT(f.health.first_redwhite, 2);
    CHECK_EQ_INT(f.particles.alive[0], 0);
    CHECK_EQ_INT(f.particles.cycle, 73);
    CHECK_EQ_INT(f.particles.velocity_base[0], 9);
    CHECK(memcmp(&boss.boss_palette, &f.assets.palette, sizeof(PALETTE)) == 0);
    CHECK(memcmp(&f.stage_palette, &boss.post_defeat, sizeof(PALETTE)) == 0);
    CHECK_EQ_INT(f.video.pages[0][0], 3);
    CHECK_EQ_INT(f.video.pages[1][0], 5);
    CHECK_EQ_INT(f.video.palette[5].red, 5 * 17);
    Arena_Free(&arena);
}

TEST(asset_validation_rejects_incomplete_and_wrong_sized_banks)
{
    ARENA arena;
    Arena_Init(&arena, "b05", 2 * 1024 * 1024);
    FIXTURE f;
    M_Init(&f, &arena);
    CHECK(Result_Ignore(B05_ValidateAssets(&f.assets)));
    f.assets.person.count = 4;
    CHECK(!Result_Ignore(B05_ValidateAssets(&f.assets)));
    f.assets.person.count = 5;
    f.assets.flash.width = 80;
    CHECK(!Result_Ignore(B05_ValidateAssets(&f.assets)));
    f.assets.flash.width = 96;
    f.assets.background.has_palette = false;
    CHECK(!Result_Ignore(B05_ValidateAssets(&f.assets)));
    Arena_Free(&arena);
}
