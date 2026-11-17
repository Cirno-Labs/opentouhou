#include "th01/boss_b10j.h"
#include "core/base/arena.h"
#include "test.h"
typedef struct {
    VIDEO video;
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
    Video_Init(&f->video, a);
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
    f->display.video = &f->video;
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
    FRAME_VSYNC vsync = { .count1 = 12, .count2 = 19 };
    B10J_BeginSetup(&b, &f.context, &vsync);
    const size_t used = arena.used;
    CHECK(!B10J_ResumeSetup(&b, &f.context, &vsync, 0));
    CHECK_EQ_INT(b.frame, 91);
    CHECK_EQ_INT(b.hp, 3);
    for (int32_t step = 0; step < 16; step++) {
        Frame_VSync(&vsync);
        CHECK(!B10J_ResumeSetup(&b, &f.context, &vsync, step + 1));
        const B10J frozen = b;
        CHECK(!B10J_ResumeSetup(&b, &f.context, &vsync, step + 1));
        CHECK(memcmp(&b, &frozen, sizeof(b)) == 0);
        CHECK_EQ_INT(b.hp, 3);
        CHECK(
            memcmp(&f.palette.saved, &f.assets.palette, sizeof(PALETTE)) == 0);
        for (int32_t i = 0; i < 16; i++) {
            const int32_t target = f.assets.palette.colors[i][0];
            const int32_t faded = 14 - step;
            CHECK_EQ_INT(
                f.video.palette[i].red, (faded > target ? faded : target) * 17);
        }
    }
    CHECK(B10J_ResumeSetup(&b, &f.context, &vsync, 17));
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
    CHECK_EQ_INT(vsync.count1, 28);
    CHECK_EQ_INT(vsync.count2, 35);
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
