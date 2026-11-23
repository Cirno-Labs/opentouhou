#include "th01/boss_b05.h"

#include "core/base/arena.h"
#include "pc98/zun/master_rng.h"

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
    HUD hud;
    PLAYER player;
    ORB orb;
    SHOTS shots;
    PELLETS pellets;
    PELLET_SHARED shared;
    STAGE_GAMEPLAY gameplay;
    GAME_TEXT_FONT font;
    PTN stage, miko;
    SCENE_DATA cache[3];
    SCENE_STATE scene;
    GAME_INPUT input;
    ROUTE_SELECT_ASSETS route_assets;
    ROUTE_SELECT_CONTEXT route_context;
    uint32_t rng, high_score;
    int16_t lives;
    uint8_t route;
    bool test_damage, cleared;
    int32_t effects[17], fades;
    uint8_t scene_at_route_effect;
    bool cleared_at_route_effect;

} FIXTURE;

static void M_Effect(void *const user, const int16_t number)
{
    FIXTURE *const f = user;
    CHECK(number >= 1 && number <= 16);
    f->effects[number]++;
    if (number == 4) {
        f->scene_at_route_effect = f->scene.active->id;
        f->cleared_at_route_effect = f->cleared;
    }
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
        Shots_RenderAction(&f->video, &f->stage, action);
    }
}

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

static void M_Battle(FIXTURE *const f, ARENA *const arena)
{
    M_Init(f, arena);
    const ALLOCATOR *const a = Arena_Allocator(arena);
    Hud_Init(&f->hud, a);
    HudHp_Init(&f->health, a);
    GAME_TEXT_GLYPH glyphs[96] = {};
    for (uint32_t i = 0; i < 90; i++) {
        glyphs[i].codepoint = 0xff01 + i;
    }
    const uint32_t others[] = {
        0x3000, 0x9b54, 0x754c, 0x3078, 0x5730, 0x7344
    };
    for (int32_t i = 0; i < 6; i++) {
        glyphs[90 + i].codepoint = others[i];
    }
    for (int32_t i = 0; i < 96; i++) {
        glyphs[i].width = 16;
        for (int32_t y = 0; y < 16; y++) {
            glyphs[i].rows[y] = glyphs[i].codepoint == 0x3000
                ? 0
                : (uint16_t)(0x8000u >> ((i + y) % 16));
        }
    }
    CHECK(Result_Ignore(GameText_LoadGlyphs(glyphs, 96, a, &f->font)));
    f->stage = (PTN) { .count = 4, .pixels = Allocator_Alloc(a, 4096) };
    memset(f->stage.pixels, PTN_TRANSPARENT, 4096);
    f->stage.pixels[3 * 1024] = 7;
    f->miko = (PTN) { .count = 1, .pixels = Allocator_Alloc(a, 1024) };
    memset(f->miko.pixels, PTN_TRANSPARENT, 1024);
    f->miko.pixels[0] = 6;
    for (uint8_t id = 0; id < 3; id++) {
        uint8_t bytes[22 + 5 * 250] = {};
        bytes[22] = (uint8_t)(8 >> id);
        READER reader;
        Reader_OpenBuffer(&reader, bytes, sizeof(bytes), nullptr);
        CHECK(Result_Ignore(Scene_LoadReader(&reader, id, a, &f->cache[id])));
        Reader_Close(&reader);
    }
    Scene_Activate(&f->scene, &f->cache[0]);
    f->route = 1;
    f->lives = 3;
    f->rng = 0xb0500000;
    f->player.invincible = true;
    f->player.left = 300;
    f->orb.previous_top = 64;
    f->display = (HUD_CONTEXT) {
        .video = &f->video,
        .sprites = &f->stage,
        .font = &f->font,
        .gameplay = &f->gameplay,
        .player = &f->player,
        .high_score = &f->high_score,
        .remaining_lives = &f->lives,
    };
    f->hit_context.hud = &f->hud;
    f->hit_context.shots = &f->shots;
    f->hit_context.shots_output = (SHOTS_OUTPUT) { M_Shot, f };
    f->hit_context.orb = &f->orb;
    f->hit_context.test_damage = &f->test_damage;
    f->hit_context.effect = M_Effect;
    f->hit_context.user = f;
    f->route_assets = (ROUTE_SELECT_ASSETS) {
        &f->font,
        &f->stage,
        &f->cache[1],
        &f->cache[2],
    };
    f->route_context = (ROUTE_SELECT_CONTEXT) {
        &f->video, &f->palette, &f->route_assets, &f->input, &f->player,
        &f->scene, &f->route,   &f->cleared,      M_Effect,  f,
    };
    f->context.pellets = &f->pellets;
    f->context.pellet_shared = &f->shared;
    f->context.rng = &f->rng;
    f->context.miko = &f->miko;
    f->context.route = &f->route_context;
    f->context.fade_music = M_Fade;
    CHECK(Result_Ignore(B05_ValidateContext(&f->context)));
}

static void M_Enter(B05 *const b, FIXTURE *const f, FRAME_VSYNC *const vsync)
{
    const TH01_INPUT raw = {};
    CHECK(!B05_UpdateAndRender(b, &f->context, &raw, vsync));
    for (int32_t frame = 0; frame < 200; frame++) {
        Frame_VSync(vsync);
        CHECK_EQ_INT(
            B05_UpdateAndRender(b, &f->context, &raw, vsync), frame == 199);
    }
    CHECK_EQ_INT(b->phase, 1);
    CHECK_EQ_INT(b->frame, 0);
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

TEST(entrance_waits_all_frames_and_leaves_world_untouched)
{
    for (int32_t reuse = 0; reuse < 2; reuse++) {
        ARENA arena;
        Arena_Init(&arena, "b05", 2 * 1024 * 1024);
        FIXTURE f;
        M_Battle(&f, &arena);
        B05 b = {};
        B05_Setup(&b, &f.context);
        const size_t used = arena.used;
        FRAME_VSYNC vsync = { .count1 = (uint16_t)reuse };
        const TH01_INPUT raw = {};
        const PLAYER player = f.player;
        const ORB orb = f.orb;
        const uint32_t rng = f.rng;
        CHECK(!B05_UpdateAndRender(&b, &f.context, &raw, &vsync));
        CHECK_EQ_INT(b.frame, 1 + reuse);
        CHECK_EQ_INT(b.hit.frame, 0);
        CHECK_EQ_INT(f.palette.saved.colors[5][0], 0);
        const B05 frozen = b;
        CHECK(!B05_UpdateAndRender(&b, &f.context, &raw, &vsync));
        CHECK(memcmp(&b, &frozen, sizeof(b)) == 0);
        for (int32_t edge = 0; edge < 200 - reuse; edge++) {
            Frame_VSync(&vsync);
            CHECK_EQ_INT(
                B05_UpdateAndRender(&b, &f.context, &raw, &vsync),
                edge == 199 - reuse);
        }
        CHECK_EQ_INT(b.phase, 1);
        CHECK_EQ_INT(b.pattern, 0);
        CHECK_EQ_INT(b.frame, 0);
        CHECK_EQ_INT(b.hit.frame, 0);
        CHECK(!b.hp_bar_filled);
        CHECK(memcmp(&f.player, &player, sizeof(player)) == 0);
        CHECK(memcmp(&f.orb, &orb, sizeof(orb)) == 0);
        CHECK_EQ_INT(f.rng, rng);
        CHECK(memcmp(&f.palette.saved, &b.boss_palette, sizeof(PALETTE)) == 0);
        CHECK_EQ_INT(arena.used, used);
        Arena_Free(&arena);
    }
}

TEST(real_hit_controller_reaches_both_cached_routes)
{
    for (int32_t rank = 0; rank < 4; rank++) {
        ARENA arena;
        Arena_Init(&arena, "b05", 2 * 1024 * 1024);
        FIXTURE f;
        M_Battle(&f, &arena);
        f.display.rank = rank;
        B05 b = {};
        B05_Setup(&b, &f.context);
        FRAME_VSYNC vsync = {};
        M_Enter(&b, &f, &vsync);
        const size_t used = arena.used;
        const TH01_INPUT raw = {};
        bool phase_two = false;
        for (int32_t step = 1; step <= 2400 && b.phase != 8; step++) {
            const bool hit =
                step == 20 || step == 70 || (step >= 1620 && step % 50 == 20);
            f.orb.previous_left = hit ? (int16_t)(b.sphere.left + 32) : 0;
            f.orb.previous_top = hit ? (int16_t)(b.sphere.top + 32) : 64;
            f.hit_context.frame_rand = (uint32_t)step;
            CHECK(B05_UpdateAndRender(&b, &f.context, &raw, &vsync));
            phase_two |= b.phase == 2;
        }
        CHECK(phase_two);
        CHECK_EQ_INT(b.phase, 8);
        CHECK_EQ_INT(b.hp, 0);
        CHECK_EQ_INT(f.gameplay.score, 24000);
        CHECK_EQ_INT(f.effects[3], 8);
        CHECK_EQ_INT(f.effects[5], 1);
        CHECK_EQ_INT(f.fades, 0);
        CHECK_EQ_INT(b.pending, B05_IDLE);
        f.pellets.slots[0] = (PELLET) {
            .moving = true,
            .left = 100 * 16,
            .top = 100 * 16,
            .previous_left = 100 * 16,
            .previous_top = 100 * 16,
        };
        f.pellets.slots[1] = (PELLET) { .cloud_frame = 7 };
        CHECK(!B05_UpdateAndRender(&b, &f.context, &raw, &vsync));
        CHECK_EQ_INT(f.fades, 1);
        CHECK(!f.pellets.slots[0].moving);
        CHECK(!f.pellets.slots[1].moving);
        CHECK_EQ_INT(f.pellets.slots[1].cloud_frame, 7);
        CHECK_EQ_INT(f.scene.active->id, 0);
        CHECK(!f.cleared && !f.player.is_hit);
        const B05 frozen = b;
        CHECK(!B05_UpdateAndRender(&b, &f.context, &raw, &vsync));
        CHECK(memcmp(&b, &frozen, sizeof(b)) == 0);
        const uint32_t rng = f.rng;
        const uint32_t frame_rand = f.hit_context.frame_rand;
        for (int32_t edge = 0; edge < 1000 && b.pending != B05_FINISHED;
             edge++) {
            const TH01_INPUT held = { .shot = true, .down = rank % 2 == 1 };
            Frame_VSync(&vsync);
            B05_UpdateAndRender(&b, &f.context, &held, &vsync);
        }
        CHECK_EQ_INT(b.pending, B05_FINISHED);
        CHECK_EQ_INT(f.fades, 1);
        CHECK_EQ_INT(f.effects[4], 1);
        CHECK_EQ_INT(f.scene_at_route_effect, 0);
        CHECK(!f.cleared_at_route_effect);
        CHECK_EQ_INT(f.route, rank % 2);
        CHECK(f.scene.active == &f.cache[1 + rank % 2]);
        CHECK(f.cleared && f.player.is_hit);
        CHECK_EQ_INT(f.rng, rng);
        CHECK_EQ_INT(f.hit_context.frame_rand, frame_rand);
        CHECK_EQ_INT(arena.used, used);
        const B05 finished = b;
        CHECK(B05_UpdateAndRender(&b, &f.context, &raw, &vsync));
        CHECK(memcmp(&b, &finished, sizeof(b)) == 0);
        CHECK_EQ_INT(f.effects[4], 1);
        Arena_Free(&arena);
    }
}

TEST(full_pool_consumes_male_rng_and_crossing_fires_late)
{
    ARENA arena;
    Arena_Init(&arena, "b05", 2 * 1024 * 1024);
    FIXTURE f;
    M_Battle(&f, &arena);
    B05 b = {};
    B05_Setup(&b, &f.context);
    b.phase = 2;
    b.sphere.left = 272;
    b.sphere.top = 96;
    b.pattern = 1;
    b.frame = 134;
    for (int32_t i = 0; i < PELLETS_COUNT; i++) {
        f.pellets.slots[i].moving = true;
    }
    const PELLETS full = f.pellets;
    uint32_t expected = f.rng;
    for (int32_t i = 0; i < 10; i++) {
        MasterRng_Next(&expected);
    }
    const TH01_INPUT raw = {};
    FRAME_VSYNC vsync = {};
    CHECK(B05_UpdateAndRender(&b, &f.context, &raw, &vsync));
    CHECK_EQ_INT(f.rng, expected);
    CHECK(memcmp(&f.pellets, &full, sizeof(full)) == 0);
    b.frame = 159;
    CHECK(B05_UpdateAndRender(&b, &f.context, &raw, &vsync));
    CHECK_EQ_INT(b.person.image, 5);
    b.frame = 163;
    CHECK(B05_UpdateAndRender(&b, &f.context, &raw, &vsync));
    CHECK_EQ_INT(b.person.image, 5);
    b.pattern = 3;
    b.frame = 184;
    expected = f.rng;
    for (int32_t i = 0; i < 20; i++) {
        MasterRng_Next(&expected);
    }
    CHECK(B05_UpdateAndRender(&b, &f.context, &raw, &vsync));
    CHECK_EQ_INT(f.rng, expected);
    memset(&f.pellets, 0, sizeof(f.pellets));
    b.pattern = 2;
    for (int32_t i = 0; i < 3; i++) {
        b.frame = (int16_t)(239 + i * 8);
        CHECK(B05_UpdateAndRender(&b, &f.context, &raw, &vsync));
        CHECK(f.pellets.slots[i * 2].moving);
        CHECK(f.pellets.slots[i * 2 + 1].moving);
    }
    Arena_Free(&arena);
}

TEST(context_validation_requires_reached_consumers)
{
    ARENA arena;
    Arena_Init(&arena, "b05", 2 * 1024 * 1024);
    FIXTURE f;
    M_Battle(&f, &arena);
    f.display.remaining_lives = nullptr;
    CHECK(!Result_Ignore(B05_ValidateContext(&f.context)));
    f.display.remaining_lives = &f.lives;
    f.hit_context.shots_output.action = nullptr;
    CHECK(!Result_Ignore(B05_ValidateContext(&f.context)));
    f.hit_context.shots_output.action = M_Shot;
    f.hit_context.hud->numbers = nullptr;
    CHECK(!Result_Ignore(B05_ValidateContext(&f.context)));
    Arena_Free(&arena);
    Arena_Init(&arena, "b05", 2 * 1024 * 1024);
    M_Battle(&f, &arena);
    f.context.fade_music = nullptr;
    CHECK(!Result_Ignore(B05_ValidateContext(&f.context)));
    Arena_Free(&arena);
}
