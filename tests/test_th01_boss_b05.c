#include "th01/boss_b05.h"
#include "th01/sound.h"
#include "th01/shots.h"
#include "th01/player.h"
#include "th01/pellets.h"
#include "th01/orb.h"
#include "th01/rng.h"
#include "th01/session.h"

#include "core/base/arena.h"
#include "pc98/zun/master_rng.h"

#include "th01_world.h"
#include "test.h"

typedef struct {
    B05_ASSETS assets;
    B05_CONTEXT context;
    GAME_TEXT_FONT font;
    PTN stage, miko;
    SCENE_DATA cache[3];
    ROUTE_SELECT_ASSETS route_assets;
    ROUTE_SELECT_CONTEXT route_context;
    uint8_t route;
    bool cleared;

} FIXTURE;

static void M_Shot(void *const user, const SHOTS_ACTION action)
{
    FIXTURE *const f = user;
    if (action.kind == SHOTS_ACTION_EFFECT) {
        Sound_Effect(action.effect);
    } else {
        Shots_RenderAction(&f->stage, action);
    }
}

static void M_Init(FIXTURE *const f, ARENA *const arena)
{
    *f = (FIXTURE) {};
    Th01Test_ResetWorld();
    GameInput_Init();
    const ALLOCATOR *const allocator = Arena_Allocator(arena);
    Video_Init(allocator);
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
    f->context = (B05_CONTEXT) {
        .assets = &f->assets,
    };
}

static void M_Battle(FIXTURE *const f, ARENA *const arena)
{
    M_Init(f, arena);
    const ALLOCATOR *const a = Arena_Allocator(arena);
    Hud_Init(a);
    HudHp_Init(a);
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
    Scene_Activate(&f->cache[0]);
    f->route = 1;
    g_Session.lives = 3;
    g_Rng = 0xb0500000;
    g_Player.invincible = true;
    g_Player.left = 300;
    g_Orb.previous_top = 64;
    Hud_Load(nullptr, &f->stage, &f->font);
    f->context.shots_output = (SHOTS_OUTPUT) { M_Shot, f };
    f->route_assets = (ROUTE_SELECT_ASSETS) {
        &f->font,
        &f->stage,
        &f->cache[1],
        &f->cache[2],
    };
    f->route_context = (ROUTE_SELECT_CONTEXT) {
        &f->route_assets,
        &f->route,
        &f->cleared,
    };
    f->context.miko = &f->miko;
    f->context.route = &f->route_context;
    g_Session.settings.difficulty = SETTINGS_DIFFICULTY_EASY;
}

static void M_Enter(FIXTURE *const f)
{
    const TH01_INPUT raw = {};
    CHECK(!B05_UpdateAndRender(&f->context, &raw));
    for (int32_t frame = 0; frame < 200; frame++) {
        Frame_VSync();
        CHECK_EQ_INT(B05_UpdateAndRender(&f->context, &raw), frame == 199);
    }
    CHECK_EQ_INT(g_B05.phase, B05_PHASE_SPHERE);
    CHECK_EQ_INT(g_B05.frame, 0);
}

TEST(setup_binds_real_banks_and_preserves_static_history)
{
    ARENA arena;
    Arena_Init(&arena, "b05", 2 * 1024 * 1024);
    FIXTURE f;
    M_Init(&f, &arena);
    memset(g_Video.pages[0], 3, VIDEO_PAGE_SIZE);
    memset(g_Video.pages[1], 5, VIDEO_PAGE_SIZE);
    const size_t used = arena.used;
    B05_Init();
    g_B05.frame = 77;
    g_B05.pattern = 3;
    g_B05.velocity_x = 999;
    g_B05.velocity_y = -4;
    g_B05.angle = 42;
    g_B05.direction = -1;
    g_B05.hp_bar_filled = true;
    g_B05.hit = (BOSS_HIT) { .frame = 23, .invincible = true };
    g_B05.sphere.previous_left = 17;
    g_B05.sphere.lock_frame = 2;
    g_B05.person.image = 5;
    g_Particles.cycle = 73;
    g_Particles.velocity_base[0] = 9;
    g_Particles.alive[0] = 255;
    B05_Setup(&f.context);
    CHECK_EQ_INT(arena.used, used);
    CHECK(g_BossSprites.entities[0] == &f.assets.sphere);
    CHECK(g_BossSprites.entities[1] == &f.assets.flash);
    CHECK(g_BossSprites.entities[2] == &f.assets.person);
    CHECK_EQ_INT(g_B05.sphere.left, 640);
    CHECK_EQ_INT(g_B05.sphere.top, 64);
    CHECK_EQ_INT(g_B05.sphere.orb_hitbox.left, 24);
    CHECK_EQ_INT(g_B05.sphere.orb_hitbox.right, 72);
    CHECK_EQ_INT(g_B05.sphere.move_clamp.right, 736);
    CHECK_EQ_INT(g_B05.sphere.image, 0);
    CHECK_EQ_INT(g_B05.sphere.previous_left, 17);
    CHECK_EQ_INT(g_B05.sphere.lock_frame, 2);
    CHECK_EQ_INT(g_B05.person.image, 5);
    CHECK_EQ_INT(g_B05.phase, B05_PHASE_START);
    CHECK_EQ_INT(g_B05.hp, 8);
    CHECK_EQ_INT(g_B05.frame, 77);
    CHECK_EQ_INT(g_B05.hit.frame, 23);
    CHECK(g_B05.hit.invincible && g_B05.hp_bar_filled);
    CHECK_EQ_INT(g_B05.pattern, 3);
    CHECK_EQ_INT(g_B05.velocity_x, 999);
    CHECK_EQ_INT(g_B05.direction, -1);
    CHECK_EQ_INT(g_HudHp.first_white, 6);
    CHECK_EQ_INT(g_HudHp.first_redwhite, 2);
    CHECK_EQ_INT(g_Particles.alive[0], 0);
    CHECK_EQ_INT(g_Particles.cycle, 73);
    CHECK_EQ_INT(g_Particles.velocity_base[0], 9);
    CHECK(memcmp(&g_B05.boss_palette, &f.assets.palette, sizeof(PALETTE)) == 0);
    CHECK(memcmp(&g_StagePalette, &g_B05.post_defeat, sizeof(PALETTE)) == 0);
    CHECK_EQ_INT(g_Video.pages[0][0], 3);
    CHECK_EQ_INT(g_Video.pages[1][0], 5);
    CHECK_EQ_INT(g_Video.palette[5].red, 5 * 17);
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
        B05_Init();
        B05_Setup(&f.context);
        const size_t used = arena.used;
        Frame_Reset();
        g_VSync.count1 = (uint16_t)reuse;
        const TH01_INPUT raw = {};
        const PLAYER player = g_Player;
        const ORB orb = g_Orb;
        const uint32_t rng = g_Rng;
        CHECK(!B05_UpdateAndRender(&f.context, &raw));
        CHECK_EQ_INT(g_B05.frame, 1 + reuse);
        CHECK_EQ_INT(g_B05.hit.frame, 0);
        CHECK_EQ_INT(g_Palette.saved.colors[5][0], 0);
        const B05 frozen = g_B05;
        CHECK(!B05_UpdateAndRender(&f.context, &raw));
        CHECK(memcmp(&g_B05, &frozen, sizeof(g_B05)) == 0);
        for (int32_t edge = 0; edge < 200 - reuse; edge++) {
            Frame_VSync();
            CHECK_EQ_INT(
                B05_UpdateAndRender(&f.context, &raw), edge == 199 - reuse);
        }
        CHECK_EQ_INT(g_B05.phase, B05_PHASE_SPHERE);
        CHECK_EQ_INT(g_B05.pattern, 0);
        CHECK_EQ_INT(g_B05.frame, 0);
        CHECK_EQ_INT(g_B05.hit.frame, 0);
        CHECK(!g_B05.hp_bar_filled);
        CHECK(memcmp(&g_Player, &player, sizeof(player)) == 0);
        CHECK(memcmp(&g_Orb, &orb, sizeof(orb)) == 0);
        CHECK_EQ_INT(g_Rng, rng);
        CHECK(
            memcmp(&g_Palette.saved, &g_B05.boss_palette, sizeof(PALETTE))
            == 0);
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
        g_Session.settings.difficulty = rank;
        g_Session.settings.difficulty = rank;
        B05_Init();
        B05_Setup(&f.context);
        Frame_Reset();
        M_Enter(&f);
        const size_t used = arena.used;
        const TH01_INPUT raw = {};
        bool phase_two = false;
        for (int32_t step = 1;
             step <= 2400 && g_B05.phase != B05_PHASE_DEFEATED; step++) {
            const bool hit =
                step == 20 || step == 70 || (step >= 1620 && step % 50 == 20);
            g_Orb.previous_left = hit ? (int16_t)(g_B05.sphere.left + 32) : 0;
            g_Orb.previous_top = hit ? (int16_t)(g_B05.sphere.top + 32) : 64;
            g_Session.frame_rand = (uint32_t)step;
            CHECK(B05_UpdateAndRender(&f.context, &raw));
            phase_two |= g_B05.phase == B05_PHASE_TRANSFORMS;
        }
        CHECK(phase_two);
        CHECK_EQ_INT(g_B05.phase, B05_PHASE_DEFEATED);
        CHECK_EQ_INT(g_B05.hp, 0);
        CHECK_EQ_INT(g_Stage.score, 24000);
        CHECK_EQ_INT(g_B05.pending, B05_IDLE);
        g_Pellets.slots[0] = (PELLET) {
            .moving = true,
            .left = 100 * 16,
            .top = 100 * 16,
            .previous_left = 100 * 16,
            .previous_top = 100 * 16,
        };
        g_Pellets.slots[1] = (PELLET) { .cloud_frame = 7 };
        CHECK(!B05_UpdateAndRender(&f.context, &raw));
        CHECK(!g_Pellets.slots[0].moving);
        CHECK(!g_Pellets.slots[1].moving);
        CHECK_EQ_INT(g_Pellets.slots[1].cloud_frame, 7);
        CHECK_EQ_INT(g_Scene.active->id, 0);
        CHECK(!f.cleared && !g_Player.is_hit);
        const B05 frozen = g_B05;
        CHECK(!B05_UpdateAndRender(&f.context, &raw));
        CHECK(memcmp(&g_B05, &frozen, sizeof(g_B05)) == 0);
        const uint32_t rng = g_Rng;
        const uint32_t frame_rand = g_Session.frame_rand;
        for (int32_t edge = 0; edge < 1000 && g_B05.pending != B05_FINISHED;
             edge++) {
            const TH01_INPUT held = { .shot = true, .down = rank % 2 == 1 };
            Frame_VSync();
            B05_UpdateAndRender(&f.context, &held);
        }
        CHECK_EQ_INT(g_B05.pending, B05_FINISHED);
        CHECK_EQ_INT(f.route, rank % 2);
        CHECK(g_Scene.active == &f.cache[1 + rank % 2]);
        CHECK(f.cleared && g_Player.is_hit);
        CHECK_EQ_INT(g_Rng, rng);
        CHECK_EQ_INT(g_Session.frame_rand, frame_rand);
        CHECK_EQ_INT(arena.used, used);
        const B05 finished = g_B05;
        CHECK(B05_UpdateAndRender(&f.context, &raw));
        CHECK(memcmp(&g_B05, &finished, sizeof(g_B05)) == 0);
        Arena_Free(&arena);
    }
}

TEST(full_pool_consumes_male_rng_and_crossing_fires_late)
{
    ARENA arena;
    Arena_Init(&arena, "b05", 2 * 1024 * 1024);
    FIXTURE f;
    M_Battle(&f, &arena);
    B05_Init();
    B05_Setup(&f.context);
    g_B05.phase = B05_PHASE_TRANSFORMS;
    g_B05.sphere.left = 272;
    g_B05.sphere.top = 96;
    g_B05.pattern = 1;
    g_B05.frame = 134;
    for (int32_t i = 0; i < PELLETS_COUNT; i++) {
        g_Pellets.slots[i].moving = true;
    }
    const PELLETS full = g_Pellets;
    uint32_t expected = g_Rng;
    for (int32_t i = 0; i < 10; i++) {
        MasterRng_Next(&expected);
    }
    const TH01_INPUT raw = {};
    Frame_Reset();
    CHECK(B05_UpdateAndRender(&f.context, &raw));
    CHECK_EQ_INT(g_Rng, expected);
    CHECK(memcmp(&g_Pellets, &full, sizeof(full)) == 0);
    g_B05.frame = 159;
    CHECK(B05_UpdateAndRender(&f.context, &raw));
    CHECK_EQ_INT(g_B05.person.image, 5);
    g_B05.frame = 163;
    CHECK(B05_UpdateAndRender(&f.context, &raw));
    CHECK_EQ_INT(g_B05.person.image, 5);
    g_B05.pattern = 3;
    g_B05.frame = 184;
    expected = g_Rng;
    for (int32_t i = 0; i < 20; i++) {
        MasterRng_Next(&expected);
    }
    CHECK(B05_UpdateAndRender(&f.context, &raw));
    CHECK_EQ_INT(g_Rng, expected);
    memset(&g_Pellets, 0, sizeof(g_Pellets));
    g_B05.pattern = 2;
    for (int32_t i = 0; i < 3; i++) {
        g_B05.frame = (int16_t)(239 + i * 8);
        CHECK(B05_UpdateAndRender(&f.context, &raw));
        CHECK(g_Pellets.slots[i * 2].moving);
        CHECK(g_Pellets.slots[i * 2 + 1].moving);
    }
    Arena_Free(&arena);
}
