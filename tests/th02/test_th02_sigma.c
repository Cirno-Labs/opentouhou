#include "tests/test.h"
#include "tests/th02/th02_boss_hp.h"
#include "tests/th02/th02_gameplay_fixture.h"
#include "tests/th02/th02_player_fixture.h"
#include "tests/th02/th02_random_fixture.h"
#include "tests/th02/th02_score_fixture.h"
#include "th02/background/private.h"
#include "th02/boss/sigma/logic.c"
#include "th02/bullets/private.h"
#include "th02/performance_private.h"

#include "core/base/memory.h"

#include <string.h>

TEST(extra_tile_midboss_runs_three_waves_and_ends_without_damage_or_rewards)
{
    FIXTURE f;
    TestGameplay_FixtureInit(&f);
    p_Stage->scroll.back = 1;
    p_Stage->scroll.front = 0;
    TestBossFight_State()->phase_frame = 0;
    TestBossFight_State()->damage = 123;
    p_Stage->world.extra_boss_frames = 99;
    const SCORE score = *g_Score;
    const uint32_t world = g_Stage->world.frame;
    const int32_t session_frame = f.session.frame;
    CHECK(!M_MidbossInvalidateCore());
    for (int frame = 1; frame <= 421; frame++) {
        M_MidbossUpdateCore(TestBossFight_State());
        CHECK_EQ_INT(BossFight_State()->phase_frame, frame == 421 ? 0 : frame);
        CHECK_EQ_INT(M_MidbossInvalidateCore(), frame <= 420);
        if (frame == 16) {
            CHECK_EQ_INT(m_Sigma.midboss_spread, 16);
            CHECK_EQ_INT(m_Sigma.tiles[0].flag, 1);
            CHECK_EQ_INT(m_Sigma.tiles[0].frame, 1);
            CHECK_EQ_INT(m_Sigma.tiles[0].position.x, 128);
            CHECK_EQ_INT(m_Sigma.tiles[0].position.y, 128);
            CHECK_EQ_INT(m_Sigma.tiles[0].group, BULLET_RING4);
        } else if (frame == 23) {
            CHECK_EQ_INT(m_Sigma.tiles[0].flag, 2);
            CHECK_EQ_INT(p_Background->cache.images[8][6], 1);
        } else if (frame == 96 || frame == 192) {
            CHECK_EQ_INT(m_Sigma.midboss_spread, 0);
        }
    }
    for (int i = 0; i < SIGMA_MIDBOSS_TILE_COUNT; i++) {
        CHECK_EQ_INT(m_Sigma.tiles[i].flag, 0);
    }
    CHECK_EQ_INT(m_Sigma.midboss_spread, 80);
    CHECK_EQ_INT(BossFight_State()->damage, 123);
    CHECK_EQ_INT(g_Stage->world.extra_boss_frames, 99);
    CHECK_EQ_INT(g_Stage->world.frame, world);
    CHECK_EQ_INT(f.session.frame, session_frame);
    CHECK(memcmp(g_Score, &score, sizeof(score)) == 0);
    Arena_Free(&f.arena);
}

TEST(extra_tile_midboss_emits_with_live_scroll_performance_and_bomb_state)
{
    for (int bombing = 0; bombing < 2; bombing++) {
        FIXTURE f;
        TestGameplay_FixtureInit(&f);
        p_Stage->scroll.back = 1;
        p_Stage->scroll.front = 0;
        p_Stage->scroll.line = 399;
        TestBossFight_State()->phase_frame = 300;
        p_Performance->value = 96;
        m_Bomb.active = bombing;
        BulletSpawn_Init(SESSION_EXTRA_RANK);
        m_Sigma.tiles[0] = (SIGMA_MIDBOSS_TILE) {
            .flag = 6,
            .frame = 7,
            .position = { 128, 32 },
            .group = BULLET_RING4,
            .speed = 100,
            .padding = 91,
        };
        M_MidbossUpdateCore(TestBossFight_State());
        CHECK_EQ_INT(m_Sigma.tiles[0].flag, 0);
        CHECK_EQ_INT(m_Sigma.tiles[0].frame, 8);
        CHECK_EQ_INT(m_Sigma.tiles[0].position.y, 33);
        CHECK_EQ_INT(m_Sigma.tiles[0].padding, 91);
        CHECK_EQ_INT(p_Background->cache.images[2][6], 9);
        CHECK_EQ_INT(g_Video.drawing_page, 1);
        for (int i = 0; i < 4; i++) {
            const BULLET *const bullet = &g_Bullets->slots[i];
            CHECK_EQ_INT(bullet->state, bombing ? ENTITY_FREE : ENTITY_ACTIVE);
            if (bombing == 0) {
                CHECK_EQ_INT(bullet->position[1].x, 132 * 16);
                CHECK_EQ_INT(bullet->position[1].y, 37 * 16);
                CHECK_EQ_INT(
                    bullet->velocity.x,
                    i == 0       ? 300
                        : i == 2 ? -300
                                 : 0);
                CHECK_EQ_INT(
                    bullet->velocity.y,
                    i == 1       ? 300
                        : i == 3 ? -300
                                 : 0);
            }
        }
        Arena_Free(&f.arena);
    }
}

TEST(sigma_entry_borrows_cached_sprites_and_resets_only_its_owned_state)
{
    FIXTURE f;
    TestGameplay_FixtureInit(&f);
    GAMEPLAY *const g = TestGameplay_State();
    SIGMA_ASSETS assets = {};
    PatternBank_Init(&assets.patterns, &g_HeapAllocator);
    uint8_t pixels[64 * 96];
    memset(pixels, 6, sizeof(pixels));
    for (uint16_t id = 128; id < 130; id++) {
        CHECK(Result_Ignore(
            PatternBank_Put(&assets.patterns, id, 64, 96, pixels, 0)));
    }
    assets.has_palette = true;
    assets.palette[0] = (GFX_COLOR) { 40, 60, 80, 255 };
    const uint8_t *const startup = g->assets.patterns.entries[0].data;
    const uint8_t *const owner = f.assets.patterns.entries[128].data;
    const GFX_COLOR dac = g_Video.palette[0];
    p_Palette->tone = 77;
    Sigma_SelectPatterns(&g->assets.patterns, &assets);
    CHECK(g->assets.patterns.entries[0].data == startup);
    CHECK(f.assets.patterns.entries[128].data == owner);
    CHECK(
        g->assets.patterns.entries[128].data
        == assets.patterns.entries[128].data);
    CHECK_EQ_INT(g->assets.patterns.next, 130);
    CHECK_EQ_INT(p_Palette->colors[0][0], 40);
    CHECK_EQ_INT(p_Palette->tone, 77);
    CHECK(memcmp(&dac, &g_Video.palette[0], sizeof(dac)) == 0);
    p_Stage->scroll.back = 1;
    p_Stage->scroll.front = 0;
    TestBossFight_State()->damage = 123;
    TestBossFight_State()->phase_frame = 99;
    TestBossFight_State()->defeated = 1;
    TestBossFight_State()->hit_flash = 7;
    TestBossFight_State()->angle = 31;
    m_Sigma.midboss_active = true;
    m_Sigma.midboss_spread = 144;
    m_Sigma.phase = SIGMA_FINAL;
    m_Sigma.circle_frame_mask = 255;
    p_Stage->world.extra_boss_frames = 900;
    m_Shots.slots[0].state = ENTITY_ACTIVE;
    m_Shots.slots[0].age = 3;
    p_Bullets->slots[0].state = ENTITY_ACTIVE;
    p_Bullets->slots[1].state = ENTITY_REMOVE;
    p_Background->cache.mode = TILE_DISABLED;
    const SCORE score = *g_Score;
    const RANDOM_STATE before_random = m_Rng;
    const PLAYER player = m_Player;
    const uint32_t world = g_Stage->world.frame;
    const int32_t session = f.session.frame;
    Sigma_Begin(TestBossFight_State());
    for (int page = 0; page < 2; page++) {
        CHECK_EQ_INT(BossFight_State()->position[page].x, 160);
        CHECK_EQ_INT(BossFight_State()->position[page].y, 48);
    }
    CHECK_EQ_INT(m_Sigma.position.x, 160);
    CHECK_EQ_INT(m_Sigma.position.y, 48);
    CHECK_EQ_INT(BossFight_State()->sprite, 128);
    CHECK_EQ_INT(BossFight_State()->phase_frame, 0);
    CHECK_EQ_INT(BossFight_State()->damage, 0);
    CHECK_EQ_INT(m_Sigma.phase, SIGMA_APPEAR);
    CHECK_EQ_INT(m_Sigma.circle_frame_mask, 7);
    CHECK_EQ_INT(BossFight_State()->defeated, 1);
    CHECK_EQ_INT(BossFight_State()->hit_flash, 7);
    CHECK_EQ_INT(BossFight_State()->angle, 31);
    CHECK(m_Sigma.midboss_active);
    CHECK_EQ_INT(m_Sigma.midboss_spread, 144);
    CHECK_EQ_INT(g_Stage->world.extra_boss_frames, 900);
    CHECK_EQ_INT(m_Shots.slots[0].state, ENTITY_FREE);
    CHECK_EQ_INT(m_Shots.slots[0].age, 3);
    CHECK_EQ_INT(g_Bullets->slots[0].state, ENTITY_REMOVE);
    CHECK_EQ_INT(g_Bullets->slots[1].state, ENTITY_FREE);
    Video_AccessPage(0);
    Sigma_DrawBody(TestBossFight_State(), &g->assets.patterns);
    CHECK_EQ_INT(g_Video.pages[0][48 * 640 + 160], 6);
    CHECK_EQ_INT(g_Video.pages[0][143 * 640 + 287], 6);
    CHECK(memcmp(g_Score, &score, sizeof(score)) == 0);
    CHECK(memcmp(&m_Rng, &before_random, sizeof(before_random)) == 0);
    CHECK(memcmp(&m_Player, &player, sizeof(player)) == 0);
    CHECK_EQ_INT(g_Stage->world.frame, world);
    CHECK_EQ_INT(f.session.frame, session);
    SIGMA retained = m_Sigma;
    g->phase = GAMEPLAY_DONE;
    g->loop.wait = STAGE_NEXT;
    Clip_Reset();
    Gameplay_BeginStage(&f.stage, &f.assets.patterns, &f.bomb);
    CHECK_EQ_INT(g_Stage->world.extra_boss_frames, 0);
    CHECK(memcmp(&m_Sigma, &retained, sizeof(retained)) == 0);
    g->assets.patterns = f.assets.patterns;
    SigmaAssets_Free(&assets);
    CHECK_EQ_INT(assets.patterns.next, 0);
    Clip_Reset();
    TestGameplay_Start(&f);
    const SIGMA empty = {};
    CHECK(memcmp(&m_Sigma, &empty, sizeof(empty)) == 0);
    Arena_Free(&f.arena);
}

TEST(sigma_background_carries_back_position_and_clears_only_the_playfield)
{
    FIXTURE f;
    TestGameplay_FixtureInit(&f);
    p_Stage->scroll.front = 0;
    p_Stage->scroll.back = 1;
    TestBossFight_State()->position[0] = (ENTITY_POINT) { 111, 88 };
    TestBossFight_State()->position[1] = (ENTITY_POINT) { -1, -2 };
    m_Sigma.position = (ENTITY_POINT) { 160, 48 };
    memset(g_Video.pages[0], 11, VIDEO_PAGE_SIZE);
    memset(g_Video.pages[1], 9, VIDEO_PAGE_SIZE);
    Video_AccessPage(1);
    M_InvalidateCore(TestBossFight_State());
    CHECK_EQ_INT(BossFight_State()->position[1].x, 111);
    CHECK_EQ_INT(BossFight_State()->position[1].y, 88);
    CHECK_EQ_INT(m_Sigma.position.x, 160);
    for (int y = 0; y < 400; y++) {
        for (int x = 0; x < 640; x++) {
            CHECK_EQ_INT(
                g_Video.pages[1][y * 640 + x],
                y >= 16 && y < 384 && x >= 32 && x < 416 ? 0 : 9);
            CHECK_EQ_INT(g_Video.pages[0][y * 640 + x], 11);
        }
    }
    CHECK_EQ_INT(g_Video.pages[1][VIDEO_PAGE_SIZE - 1], 9);
    CHECK_EQ_INT(g_Video.drawing_page, 1);
    CHECK_EQ_INT(g_Grcg->mode, 0);
    Arena_Free(&f.arena);
}

TEST(sigma_live_hit_draws_the_hit_sprite_then_restores_its_pattern)
{
    FIXTURE f;
    TestGameplay_FixtureInit(&f);
    GAMEPLAY *const g = TestGameplay_State();
    p_Stage->scroll.back = 1;
    p_Stage->scroll.front = 0;
    Video_AccessPage(1);
    Sigma_Begin(TestBossFight_State());
    uint8_t pixels[64 * 96];
    memset(pixels, 7, sizeof(pixels));
    for (uint16_t id = 130; id < 132; id++) {
        CHECK(Result_Ignore(
            PatternBank_Put(&f.assets.patterns, id, 64, 96, pixels, 0)));
        g->assets.patterns.entries[id] = f.assets.patterns.entries[id];
    }
    m_Shots.weapon.level = 3;
    SHOT *const shot = &m_Shots.slots[0];
    shot->state = ENTITY_ACTIVE;
    shot->sprite = 50;
    shot->position[1] = (ENTITY_POINT) { 200 * 16, 88 * 16 };
    shot->velocity.y = -128;
    CHECK_EQ_INT(
        M_UpdateCore(
            TestBossFight_State(), &g->assets.patterns,
            f.session.reduce_effects),
        1);
    CHECK_EQ_INT(BossFight_State()->damage, 5);
    CHECK_EQ_INT(m_Score.delta, 5);
    CHECK_EQ_INT(BossFight_State()->sprite, 128);
    CHECK_EQ_INT(BossFight_State()->hit_flash, 0);
    CHECK_EQ_INT(shot->age, 2);
    CHECK_EQ_INT(g_Stage->world.extra_boss_frames, 1);
    CHECK_EQ_INT(g_Video.pages[1][48 * 640 + 160], 7);
    Arena_Free(&f.arena);
}

TEST(sigma_defeat_awards_directly_and_excludes_its_animation_from_fight_time)
{
    FIXTURE f;
    TestGameplay_FixtureInit(&f);
    GAMEPLAY *const g = TestGameplay_State();
    p_Stage->scroll.back = 1;
    p_Stage->scroll.front = 0;
    p_Stage->world.frame = 1;
    Video_AccessPage(1);
    Sigma_Begin(TestBossFight_State());
    m_Sigma.phase = SIGMA_FINAL;
    m_Sigma.attacks[0] = SIGMA_ATTACK_FAN_SWEEP;
    m_Sigma.phase_damage = 5000;
    TestBossFight_State()->damage = 1300;
    m_Score.value = 123;
    m_Score.delta = 17;
    p_Stage->world.extra_boss_frames = UINT32_MAX;
    m_Player.invincibility = 77;
    CHECK_EQ_INT(
        M_UpdateCore(
            TestBossFight_State(), &g->assets.patterns,
            f.session.reduce_effects),
        1);
    CHECK_EQ_INT(BossFight_State()->defeated, 1);
    CHECK_EQ_INT(BossFight_State()->phase_frame, 0);
    CHECK_EQ_INT(m_Score.value, 300123);
    CHECK_EQ_INT(m_Score.delta, 17);
    CHECK_EQ_INT(g_Stage->world.extra_boss_frames, 0);
    CHECK_EQ_INT(m_Player.invincibility, 77);
    m_Player.position[0] = (ENTITY_POINT) { 224, 390 };
    for (int frame = 1; frame <= 224; frame++) {
        m_Player.hit = 0;
        CHECK_EQ_INT(
            M_UpdateCore(
                TestBossFight_State(), &g->assets.patterns,
                f.session.reduce_effects),
            frame == 224 ? 2 : 1);
        CHECK_EQ_INT(g_Stage->world.extra_boss_frames, 0);
        CHECK_EQ_INT(m_Score.value, 300123);
        if (frame == 101) {
            CHECK_EQ_INT(m_Player.hit, 1);
        } else if (frame == 141) {
            CHECK_EQ_INT(g_Stage->world.slowdown, 2);
        } else if (frame == 180) {
            CHECK_EQ_INT(g_Stage->world.slowdown, 1);
        }
    }
    CHECK_EQ_INT(BossFight_State()->sprite, 140);
    CHECK_EQ_INT(g_Stage->world.frame, 1);
    Arena_Free(&f.arena);
}

TEST(sigma_circles_advance_on_background_and_change_hit_radius_before_testing)
{
    FIXTURE f;
    TestGameplay_FixtureInit(&f);
    GAMEPLAY *const g = TestGameplay_State();
    p_Stage->scroll.back = 0;
    p_Stage->scroll.front = 1;
    p_Stage->world.frame = 1;
    Video_AccessPage(0);
    Sigma_Begin(TestBossFight_State());
    m_Sigma.phase = 255;
    m_Sigma.circle_hitbox_shrink = 4;
    SIGMA_CIRCLE *const c = &m_Sigma.circles[0];
    *c = (SIGMA_CIRCLE) { .flag = 2,
                          .frame = 32,
                          .center = { 240, 200 },
                          .radius_max = 64,
                          .radius = 64 };
    m_Player.position[1] = (ENTITY_POINT) { 224, 184 };
    M_InvalidateCore(TestBossFight_State());
    CHECK_EQ_INT(c->flag, 3);
    CHECK_EQ_INT(
        M_UpdateCore(
            TestBossFight_State(), &g->assets.patterns,
            f.session.reduce_effects),
        1);
    CHECK_EQ_INT(c->flag, 4);
    CHECK_EQ_INT(c->frame, 0);
    CHECK_EQ_INT(c->radius_max, 64);
    CHECK_EQ_INT(c->radius, 8);
    CHECK_EQ_INT(m_Player.hit, 0);
    CHECK_EQ_INT(
        M_UpdateCore(
            TestBossFight_State(), &g->assets.patterns,
            f.session.reduce_effects),
        1);
    CHECK_EQ_INT(c->radius, 24);
    CHECK_EQ_INT(g_Video.pages[0][200 * 640 + 240], 0);
    p_Stage->scroll.back = 1;
    p_Stage->scroll.front = 0;
    CHECK_EQ_INT(
        M_UpdateCore(
            TestBossFight_State(), &g->assets.patterns,
            f.session.reduce_effects),
        1);
    CHECK_EQ_INT(c->radius, 24);
    p_Stage->scroll.back = 0;
    p_Stage->scroll.front = 1;
    for (int frame = 0; frame < 4; frame++) {
        CHECK_EQ_INT(
            M_UpdateCore(
                TestBossFight_State(), &g->assets.patterns,
                f.session.reduce_effects),
            1);
    }
    CHECK_EQ_INT(c->flag, 0);
    Arena_Free(&f.arena);
}

TEST(sigma_sweep_reads_x_before_speed_and_keeps_the_frame_start_origin)
{
    FIXTURE f;
    TestGameplay_FixtureInit(&f);
    GAMEPLAY *const g = TestGameplay_State();
    p_Stage->scroll.back = 1;
    p_Stage->scroll.front = 0;
    Video_AccessPage(1);
    Sigma_Begin(TestBossFight_State());
    p_Stage->world.frame = 1;
    m_Sigma.phase = SIGMA_OPENING;
    m_Sigma.attacks[0] = SIGMA_ATTACK_SWEEP;
    m_Sigma.sweep_velocity = 1;
    TestBossFight_State()->phase_frame = 20;
    p_Performance->value = 0;
    BulletSpawn_Init(SESSION_EXTRA_RANK);
    m_Rng.cursor = 0;
    m_Rng.ring[0] = 17;
    m_Rng.ring[1] = 3;
    CHECK_EQ_INT(
        M_UpdateCore(
            TestBossFight_State(), &g->assets.patterns,
            f.session.reduce_effects),
        1);
    CHECK_EQ_INT(m_Rng.cursor, 2);
    CHECK_EQ_INT(BossFight_State()->position[1].x, 161);
    CHECK_EQ_INT(m_Sigma.position.x, 161);
    CHECK_EQ_INT(m_Sigma.bullet_origin.x, 220);
    CHECK_EQ_INT(m_Shots.target.x, 220);
    CHECK_EQ_INT(g_Bullets->slots[0].state, ENTITY_ACTIVE);
    CHECK_EQ_INT(g_Bullets->slots[0].position[1].x, 177 * 16);
    CHECK_EQ_INT(g_Bullets->slots[0].position[1].y, 112 * 16);
    CHECK_EQ_INT(g_Bullets->slots[0].velocity.x, 0);
    CHECK_EQ_INT(g_Bullets->slots[0].velocity.y, 33);
    Arena_Free(&f.arena);
}

TEST(sigma_dot_rings_apply_live_scroll_after_the_unrolled_point_guard)
{
    FIXTURE f;
    TestGameplay_FixtureInit(&f);
    GAMEPLAY *const g = TestGameplay_State();
    p_Stage->scroll.back = 1;
    p_Stage->scroll.front = 0;
    Video_AccessPage(1);
    Sigma_Begin(TestBossFight_State());
    m_Sigma.phase = 255;
    p_Stage->scroll.line = 399;
    p_Stage->world.frame = 0;
    memset(g_Video.pages[1], 0, VIDEO_PAGE_SIZE);
    CHECK_EQ_INT(
        M_UpdateCore(
            TestBossFight_State(), &g->assets.patterns,
            f.session.reduce_effects),
        1);
    CHECK_EQ_INT(m_Sigma.ring_angle, 8);
    CHECK_EQ_INT(g_Video.pages[1][199 * 640 + 232], 4);
    CHECK_EQ_INT(g_Video.pages[1][200 * 640 + 232], 0);
    Arena_Free(&f.arena);
}

TEST(sigma_hp_hook_reapplies_at_phase_resets_and_uses_final_defeat_threshold)
{
    FIXTURE f;
    TestGameplay_FixtureInit(&f);
    TEST_BOSS_HP hp;
    TestBossHp_Begin(&hp);
    BOSS_STATE *const boss = TestBossFight_State();
    Sigma_Begin(boss);
    CHECK_EQ_INT(boss->damage, 1799);
    static const SIGMA_PHASE phases[] = {
        SIGMA_OPENING,
        SIGMA_ORBIT,
        SIGMA_BOUNCE,
        SIGMA_TRACKING,
    };
    for (size_t i = 0; i < ARRAY_SIZE(phases); i++) {
        m_Sigma.phase = phases[i];
        m_Sigma.pattern = 3;
        m_Sigma.phase_damage = 1800;
        boss->phase_frame = 0;
        boss->damage = 1800;
        M_Phases(boss);
        CHECK_EQ_INT(boss->damage, i == 3 ? 1299 : 1799);
        CHECK_EQ_INT(hp.count, i + 2);
    }
    m_Sigma.phase = SIGMA_FINAL;
    m_Sigma.pattern = 3;
    boss->phase_frame = 1;
    boss->damage = 1300;
    M_Phases(boss);
    CHECK_EQ_INT(boss->defeated, 1);
    CHECK_EQ_INT(hp.count, 5);
    CHECK_EQ_INT(hp.thresholds[4], 1300);
    TestBossHp_End();
    Arena_Free(&f.arena);
}
