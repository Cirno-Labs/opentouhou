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
