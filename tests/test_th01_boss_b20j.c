#include "th01/boss_b20j.h"

#include "core/base/arena.h"

#include "test.h"

static void M_Assets(B20J_ASSETS *const a, ARENA *const arena)
{
    *a = (B20J_ASSETS) {};
    a->head = (BOS) { 80, 80, 3, Arena_AllocZ(arena, 80 * 80 * 3),
                      Arena_AllocZ(arena, 80 * 80 * 3) };
    a->face = (BOS) { 80, 32, 6, Arena_AllocZ(arena, 80 * 32 * 6),
                      Arena_AllocZ(arena, 80 * 32 * 6) };
    a->aim = (BOS) { 80, 32, 3, Arena_AllocZ(arena, 80 * 32 * 3),
                     Arena_AllocZ(arena, 80 * 32 * 3) };
    a->body.count = 16;
    uint8_t *const pixels = Arena_AllocZ(arena, VIDEO_PAGE_SIZE);
    uint8_t *const coverage = Arena_AllocZ(arena, VIDEO_PAGE_SIZE);
    for (int32_t i = 0; i < 16; i++) {
        a->body.images[i] =
            (GRX) { .stream_count = 1, .pixels = pixels, .coverage = coverage };
    }
    PI_IMAGE *const images[] = { &a->scroll,    &a->upper,     &a->layers[0],
                                 &a->layers[1], &a->layers[2], &a->layers[3] };
    for (int32_t i = 0; i < 6; i++) {
        *images[i] = (PI_IMAGE) {
            .width = 640, .height = 400, .has_palette = true, .pixels = pixels
        };
    }
    a->music.bytes = pixels;
    a->effects.bytes = pixels;
    a->diamond = pixels;
    a->flake = pixels;
}

TEST(setup_resets_hp_and_face_but_preserves_history)
{
    ARENA arena;
    Arena_Init(&arena, "Konngara", 1024 * 1024);
    B20J_ASSETS assets;
    M_Assets(&assets, &arena);
    BOSS_SPRITES sprites = {};
    PALETTE_STATE palette = {};
    palette.saved.colors[10][1] = 7;
    HUD_HP health = {};
    const BOSS_HIT_CONTEXT hit = { .palette = &palette, .health = &health };
    const B20J_CONTEXT c = { .assets = &assets,
                             .sprites = &sprites,
                             .hit = &hit };
    B20J b;
    B20J_Init(&b);
    b.hit = (BOSS_HIT) { 27, true };
    b.pattern_state = 32;
    b.diamond_edges_frame = 91;
    b.two_snakes.left[1][3] = 172;
    b.ray_previous_x = 44;
    b.finished = true;
    const size_t used = arena.used;
    B20J_Setup(&b, &c);
    CHECK_EQ_INT(arena.used, used);
    CHECK_EQ_INT(b.hp, 18);
    CHECK_EQ_INT(b.phase, 0);
    CHECK_EQ_INT(b.face_direction, 2);
    CHECK_EQ_INT(b.face_expression, 0);
    CHECK(b.direction_can_change);
    CHECK(!b.finished);
    CHECK_EQ_INT(health.first_white, 16);
    CHECK_EQ_INT(health.first_redwhite, 10);
    CHECK_EQ_INT(b.boss_palette.colors[10][1], 7);
    CHECK_EQ_INT(b.hit.frame, 27);
    CHECK(b.hit.invincible);
    CHECK_EQ_INT(b.pattern_state, 32);
    CHECK_EQ_INT(b.diamond_edges_frame, 91);
    CHECK_EQ_INT(b.two_snakes.left[1][3], 172);
    CHECK_EQ_INT(b.ray_previous_x, 44);
    CHECK_EQ_INT(b.head.sprites.width_bytes, 10);
    Arena_Free(&arena);
}

TEST(validation_requires_all_sixteen_images_and_audio)
{
    ARENA arena;
    Arena_Init(&arena, "Konngara validation", 1024 * 1024);
    B20J_ASSETS a;
    M_Assets(&a, &arena);
    CHECK(Result_Ignore(B20J_ValidateAssets(&a)));
    a.body.count = 7;
    CHECK(!Result_Ignore(B20J_ValidateAssets(&a)));
    a.body.count = 16;
    a.body.images[15].coverage = nullptr;
    CHECK(!Result_Ignore(B20J_ValidateAssets(&a)));
    a.body.images[15].coverage = a.body.images[0].coverage;
    a.effects.bytes = nullptr;
    CHECK(!Result_Ignore(B20J_ValidateAssets(&a)));
    Arena_Free(&arena);
}
