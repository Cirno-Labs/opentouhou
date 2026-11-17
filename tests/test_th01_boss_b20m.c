#include "th01/boss_b20m.h"
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
