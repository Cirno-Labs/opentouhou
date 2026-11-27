#include "tests/test.h"
#include "tests/th02/th02_gameplay_fixture.h"
#include "tests/th02/th02_player_fixture.h"
#include "tests/th02/th02_random_fixture.h"
#include "tests/th02/th02_score_fixture.h"
#include "th02/background/private.h"
#include "th02/boss/meira.c"
#include "th02/bullets/private.h"
#include "th02/performance_private.h"

#include <string.h>

static void M_MeiraStep(FIXTURE *const f, const int16_t frame)
{
    TestBossFight_State()->phase_frame = frame;
    CHECK_EQ_INT(
        M_UpdateCore(
            TestBossFight_State(), &TestGameplay_State()->assets.patterns,
            (uint8_t)f->session.rank),
        BOSS_RUNNING);
}

static void M_MeiraDash(FIXTURE *const f, const unsigned flowers)
{
    TestBossFight_State()->sprite = 141;
    M_Place(TestBossFight_State());
    M_BeginCore(TestBossFight_State(), (uint8_t)f->session.rank);
    p_Player->current = (ENTITY_POINT) { 0, 336 };
    M_MeiraStep(f, 90);
    M_MeiraStep(f, 19);
    for (unsigned i = 0; i < flowers; i++) {
        M_MeiraStep(f, (int16_t)(20 + i));
    }
}

static void M_MeiraFlight(FIXTURE *const f)
{
    M_BeginCore(TestBossFight_State(), (uint8_t)f->session.rank);
    TestBossFight_State()->damage = 701;
    M_MeiraStep(f, 90);
    TestBossFight_State()->position[g_Stage->scroll.back] =
        (ENTITY_POINT) { 16, 48 };
    M_MeiraStep(f, 80);
    TestBossFight_State()->damage = 1501;
    M_MeiraStep(f, 119);
    M_MeiraStep(f, 98);
    CHECK_EQ_INT(m_Meira.phase, MEIRA_BILLIARDS);
    CHECK_EQ_INT(m_Meira.pattern, 1);
    TestBossFight_State()->damage = 0;
}

TEST(stage_two_midboss_enters_through_modules_and_activates_lightning)
{
    FIXTURE f;
    TestGameplay_FixtureInit(&f);
    GAMEPLAY *const game = TestGameplay_State();
    f.session.stage = 1;
    Stage_Begin(1, f.session.reduce_effects);
    TestBossFight_State()->position[0].y = 41;
    TestBossFight_State()->defeated = 7;
    TestBossFight_State()->phase_frame = 320;
    TestBossFight_State()->position[0] = TestBossFight_State()->position[1] =
        (ENTITY_POINT) { 192, 64 };
    for (int frame = 0; frame < 13; frame++) {
        M_MidbossUpdateCore(TestBossFight_State(), &game->assets.patterns);
    }
    TestBossFight_State()->phase_frame = 0;
    TestBossFight_State()->position[0].y = 41;
    p_Stage->world.midboss_active = 1;
    const uint32_t world = g_Stage->world.frame;
    const int32_t session = f.session.frame;
    BossFight_MidbossUpdate(
        &game->assets.patterns, f.session.stage, (uint8_t)f.session.rank);
    CHECK(m_Midboss.active);
    CHECK_EQ_INT(BossFight_State()->phase_frame, 1);
    CHECK_EQ_INT(BossFight_State()->position[0].x, 192);
    CHECK_EQ_INT(BossFight_State()->position[1].x, 192);
    CHECK_EQ_INT(BossFight_State()->position[0].y, -16);
    CHECK_EQ_INT(BossFight_State()->position[1].y, -16);
    CHECK_EQ_INT(BossFight_State()->defeated, 7);
    CHECK_EQ_INT(m_Midboss.kill_frame, 13);
    CHECK_EQ_INT(p_Shots->target.x, 216);
    CHECK_EQ_INT(p_Shots->target.y, 65);
    CHECK_EQ_INT(g_StageEffects->lightning_frame, 1);
    CHECK(BossFight_MidbossInvalidate(f.session.stage));
    StageEffects_Update(
        StageScroll_Pages(), g_Stage->index, g_Stage->reduce_effects,
        &p_Stage->world.slowdown);
    CHECK_EQ_INT(g_StageEffects->lightning_frame, 2);
    CHECK_EQ_INT(g_StageEffects->lightning_next, 100);
    CHECK_EQ_INT(g_Sparks->acceleration_x, -1);
    CHECK_EQ_INT(g_Stage->world.frame, world);
    CHECK_EQ_INT(f.session.frame, session);
    Arena_Free(&f.arena);
}

TEST(
    stage_two_midboss_counts_early_hits_once_but_keeps_raw_score_and_shot_aging)
{
    FIXTURE f;
    TestGameplay_FixtureInit(&f);
    GAMEPLAY *const game = TestGameplay_State();
    TestBossFight_State()->position[0] = TestBossFight_State()->position[1] =
        (ENTITY_POINT) { 192, 64 };
    TestBossFight_State()->phase_frame = 100;
    TestBossFight_State()->damage = 70;
    p_Stage->world.frame = 2;
    p_Shots->weapon.level = 0;
    SHOT *const shot = &p_Shots->slots[0];
    shot->state = ENTITY_ACTIVE;
    shot->sprite = 50;
    shot->position[p_Stage->scroll.back] = (ENTITY_POINT) { 208 * 16, 80 * 16 };
    M_MidbossUpdateCore(TestBossFight_State(), &game->assets.patterns);
    CHECK_EQ_INT(BossFight_State()->damage, 71);
    CHECK_EQ_INT(p_Score->delta, 6);
    CHECK_EQ_INT(shot->age, 2);
    CHECK_EQ_INT(g_Sparks->next, 1);
    M_MidbossUpdateCore(TestBossFight_State(), &game->assets.patterns);
    CHECK_EQ_INT(BossFight_State()->damage, 71);
    CHECK_EQ_INT(p_Score->delta, 6);
    TestBossFight_State()->phase_frame = 319;
    shot->age = 0;
    M_MidbossUpdateCore(TestBossFight_State(), &game->assets.patterns);
    CHECK_EQ_INT(BossFight_State()->phase_frame, 320);
    CHECK_EQ_INT(BossFight_State()->damage, 77);
    CHECK_EQ_INT(p_Score->delta, 12);
    CHECK_EQ_INT(shot->age, 2);
    CHECK_EQ_INT(g_Sparks->next, 2);
    Arena_Free(&f.arena);
}

TEST(stage_two_midboss_defeat_clears_shared_flag_after_three_sprite_bursts)
{
    FIXTURE f;
    TestGameplay_FixtureInit(&f);
    GAMEPLAY *const game = TestGameplay_State();
    f.session.stage = 1;
    TestBossFight_State()->position[0] = TestBossFight_State()->position[1] =
        (ENTITY_POINT) { 192, 64 };
    M_MidbossUpdateCore(TestBossFight_State(), &game->assets.patterns);
    TestBossFight_State()->position[0] = TestBossFight_State()->position[1] =
        (ENTITY_POINT) { 192, 64 };
    TestBossFight_State()->phase_frame = 320;
    TestBossFight_State()->damage = 380;
    p_Stage->scroll.line = 240;
    p_Bomb->active = 1;
    p_Stage->world.midboss_active = 1;
    p_Player->invincibility = 17;
    M_MidbossUpdateCore(TestBossFight_State(), &game->assets.patterns);
    CHECK_EQ_INT(BossFight_State()->damage, 381);
    CHECK_EQ_INT(BossFight_State()->defeated, 0);
    CHECK_EQ_INT(p_Score->delta, 1);
    p_Stage->scroll.line = 239;
    M_MidbossUpdateCore(TestBossFight_State(), &game->assets.patterns);
    CHECK_EQ_INT(BossFight_State()->damage, 382);
    CHECK_EQ_INT(BossFight_State()->defeated, 1);
    CHECK_EQ_INT(p_Score->delta, 20002);
    const uint16_t next = g_Sparks->next;
    unsigned expected = 0;
    for (int frame = 1; frame <= 64; frame++) {
        CHECK(M_MidbossInvalidateCore(TestBossFight_State()));
        M_MidbossUpdateCore(TestBossFight_State(), &game->assets.patterns);
        if (frame == 6 || frame == 22 || frame == 38) {
            expected += 24;
        }
        CHECK_EQ_INT(g_Sparks->next, (next + expected) % SPARK_COUNT);
        CHECK_EQ_INT(m_Midboss.active, frame != 64);
        CHECK_EQ_INT(m_Midboss.kill_frame, frame == 64 ? 0 : frame);
        StageScroll_Swap();
    }
    CHECK_EQ_INT(expected, 72);
    CHECK_EQ_INT(BossFight_State()->phase_frame, 0);
    CHECK_EQ_INT(BossFight_State()->defeated, 0);
    CHECK_EQ_INT(p_Shots->target.x, -1);
    CHECK_EQ_INT(p_Shots->target.y, -1);
    CHECK_EQ_INT(p_Player->invincibility, 17);
    CHECK_EQ_INT(p_Score->delta, 20002);
    CHECK_EQ_INT(g_Stage->world.midboss_active, 1);
    CHECK(!BossFight_MidbossInvalidate(f.session.stage));
    Arena_Free(&f.arena);
}

TEST(stage_two_midboss_chase_bullets_read_live_performance_page_and_bomb)
{
    FIXTURE f;
    TestGameplay_FixtureInit(&f);
    GAMEPLAY *const game = TestGameplay_State();
    f.session.rank = 1;
    BulletSpawn_Init(1);
    TestBossFight_State()->position[0] = TestBossFight_State()->position[1] =
        (ENTITY_POINT) { 192, 64 };
    TestBossFight_State()->phase_frame = 383;
    p_Performance->value = 48;
    const RANDOM_STATE before_random = *p_Rng;
    M_MidbossUpdateCore(TestBossFight_State(), &game->assets.patterns);
    CHECK_EQ_INT(g_Bullets->slots[0].state, ENTITY_ACTIVE);
    CHECK_EQ_INT(g_Bullets->slots[0].motion, BULLET_MOTION_CHASE);
    CHECK_EQ_INT(g_Bullets->slots[0].sprite, 84);
    CHECK_EQ_INT(g_Bullets->slots[0].speed, 60);
    CHECK_EQ_INT(g_Bullets->slots[0].position[0].x, 220 * 16);
    CHECK_EQ_INT(g_Bullets->slots[0].position[0].y, 96 * 16);
    TestBossFight_State()->phase_frame = 391;
    p_Performance->value = 24;
    p_Stage->scroll.back = 1;
    M_MidbossUpdateCore(TestBossFight_State(), &game->assets.patterns);
    CHECK_EQ_INT(g_Bullets->slots[1].state, ENTITY_ACTIVE);
    CHECK_EQ_INT(g_Bullets->slots[1].speed, 45);
    CHECK_EQ_INT(g_Bullets->slots[1].position[1].x, 220 * 16);
    TestBossFight_State()->phase_frame = 399;
    p_Bomb->active = 1;
    M_MidbossUpdateCore(TestBossFight_State(), &game->assets.patterns);
    CHECK_EQ_INT(g_Bullets->slots[2].state, ENTITY_FREE);
    CHECK(memcmp(p_Rng, &before_random, sizeof(before_random)) == 0);
    Arena_Free(&f.arena);
}

TEST(stage_two_midboss_timeout_waits_for_no_hit_without_award_or_sound)
{
    FIXTURE f;
    TestGameplay_FixtureInit(&f);
    GAMEPLAY *const game = TestGameplay_State();
    TEST_SOUND sound;
    TestSound_Init(&sound);
    TestBossFight_State()->position[0] = TestBossFight_State()->position[1] =
        (ENTITY_POINT) { 192, 64 };
    TestBossFight_State()->phase_frame = 1600;
    TestBossFight_State()->damage = 380;
    p_Stage->scroll.line = 240;
    p_Bomb->active = 1;
    p_Stage->world.frame = 2;
    M_MidbossUpdateCore(TestBossFight_State(), &game->assets.patterns);
    CHECK_EQ_INT(BossFight_State()->defeated, 0);
    CHECK_EQ_INT(BossFight_State()->damage, 381);
    CHECK_EQ_INT(p_Score->delta, 1);
    SoundEffects_Update();
    TestSound_Check(&sound, 4);
    p_Stage->world.frame = 3;
    M_MidbossUpdateCore(TestBossFight_State(), &game->assets.patterns);
    CHECK_EQ_INT(BossFight_State()->defeated, 1);
    CHECK_EQ_INT(p_Score->delta, 1);
    CHECK_EQ_INT(m_Midboss.kill_frame, 0);
    SoundEffects_Update();
    TestSound_Check(&sound, -1);
    TestSound_Free(&sound);
    Arena_Free(&f.arena);
}

TEST(meira_entry_preserves_clocks_flowers_and_attack_history)
{
    FIXTURE f;
    TestGameplay_FixtureInit(&f);
    GAMEPLAY *const game = TestGameplay_State();
    f.session.stage = 1;
    p_Stage->world.frame = 900;
    TestBossFight_State()->phase_frame = 77;
    M_MeiraDash(&f, 2);
    M_MeiraFlight(&f);
    M_MeiraStep(&f, 49);
    TestBossFight_State()->defeated = 1;
    for (int frame = 0; frame < 9; frame++) {
        CHECK_EQ_INT(
            M_UpdateCore(
                TestBossFight_State(), &game->assets.patterns,
                (uint8_t)f.session.rank),
            BOSS_RUNNING);
    }
    const MEIRA before = m_Meira;
    CHECK_EQ_INT(before.kill_frame, 9);
    CHECK(before.trail_active);
    CHECK_EQ_INT(before.flowers[0].flag, ENTITY_ACTIVE);
    TestBossFight_State()->phase_frame = 77;
    TestBossFight_State()->damage = 999;
    TestBossFight_State()->defeated = 1;
    TestBossFight_State()->sprite = 141;
    M_Place(TestBossFight_State());
    M_BeginCore(TestBossFight_State(), (uint8_t)f.session.rank);
    CHECK_EQ_INT(BossFight_State()->position[0].x, 192);
    CHECK_EQ_INT(BossFight_State()->position[1].y, 48);
    CHECK_EQ_INT(g_Stage->world.frame, 900);
    CHECK_EQ_INT(BossFight_State()->phase_frame, 77);
    CHECK_EQ_INT(BossFight_State()->damage, 0);
    CHECK_EQ_INT(BossFight_State()->defeated, 0);
    CHECK_EQ_INT(BossFight_State()->angle, 0);
    CHECK_EQ_INT(g_Bullets->turn_limit, 2);
    CHECK_EQ_INT(m_Meira.kill_frame, 9);
    CHECK_EQ_INT(m_Meira.flower_fire_i, before.flower_fire_i);
    CHECK_EQ_INT(m_Meira.flower_speed, before.flower_speed);
    CHECK_EQ_INT(m_Meira.direction, before.direction);
    CHECK_EQ_INT(m_Meira.flowers[0].flag, ENTITY_ACTIVE);
    CHECK(memcmp(m_Meira.flowers, before.flowers, sizeof(before.flowers)) == 0);
    CHECK(memcmp(m_Meira.trail, before.trail, sizeof(before.trail)) == 0);
    CHECK(!m_Meira.trail_active);
    CHECK_EQ_INT(m_Meira.fan_pattern, BULLET_FAN3_WIDE);
    f.session.rank = 1;
    M_BeginCore(TestBossFight_State(), (uint8_t)f.session.rank);
    CHECK_EQ_INT(m_Meira.fan_pattern, BULLET_FAN5_WIDE);
    CHECK_EQ_INT(m_Meira.burst_pattern, BULLET_FAN5_MEDIUM_AIMED);
    TestBossFight_State()->phase_frame = 0;
    M_MidbossUpdateCore(TestBossFight_State(), &game->assets.patterns);
    CHECK(m_Midboss.active);
    f.stage.stage = (uint8_t)f.session.stage;
    TestGameplay_Start(&f);
    CHECK_EQ_INT(m_Meira.kill_frame, 0);
    CHECK_EQ_INT(m_Meira.flower_fire_i, 0);
    CHECK_EQ_INT(m_Meira.flowers[0].flag, ENTITY_FREE);
    CHECK(!m_Meira.trail_active);
    CHECK(!m_Midboss.active);
    Arena_Free(&f.arena);
}

TEST(meira_changes_phase_only_after_the_source_pattern_completes)
{
    FIXTURE f;
    TestGameplay_FixtureInit(&f);
    GAMEPLAY *const game = TestGameplay_State();
    TestBossFight_State()->sprite = 141;
    M_Place(TestBossFight_State());
    M_BeginCore(TestBossFight_State(), (uint8_t)f.session.rank);
    TestBossFight_State()->damage = 701;
    TestBossFight_State()->phase_frame = 90;
    CHECK_EQ_INT(
        M_UpdateCore(
            TestBossFight_State(), &game->assets.patterns,
            (uint8_t)f.session.rank),
        1);
    CHECK_EQ_INT(BossFight_State()->phase_frame, 0);
    CHECK_EQ_INT(m_Meira.pattern, 1);
    CHECK_EQ_INT(m_Meira.phase, MEIRA_OPENING);
    TestBossFight_State()->damage = 700;
    TestBossFight_State()->position[g_Stage->scroll.back] =
        (ENTITY_POINT) { 16, 48 };
    M_MeiraStep(&f, 80);
    CHECK_EQ_INT(m_Meira.pattern, 2);
    TestBossFight_State()->position[p_Stage->scroll.back].x = 192;
    M_MeiraStep(&f, 155);
    CHECK_EQ_INT(m_Meira.pattern, 3);
    TestBossFight_State()->damage = 701;
    TestBossFight_State()->phase_frame = 115;
    CHECK_EQ_INT(
        M_UpdateCore(
            TestBossFight_State(), &game->assets.patterns,
            (uint8_t)f.session.rank),
        1);
    CHECK_EQ_INT(BossFight_State()->phase_frame, 0);
    CHECK_EQ_INT(m_Meira.pattern, 0);
    CHECK_EQ_INT(m_Meira.phase, MEIRA_PRESSURE);
    TestBossFight_State()->damage = 1501;
    TestBossFight_State()->phase_frame = 118;
    CHECK_EQ_INT(
        M_UpdateCore(
            TestBossFight_State(), &game->assets.patterns,
            (uint8_t)f.session.rank),
        1);
    CHECK_EQ_INT(m_Meira.phase, MEIRA_PRESSURE);
    CHECK_EQ_INT(
        M_UpdateCore(
            TestBossFight_State(), &game->assets.patterns,
            (uint8_t)f.session.rank),
        1);
    CHECK_EQ_INT(BossFight_State()->phase_frame, 0);
    CHECK_EQ_INT(m_Meira.pattern, 0);
    CHECK_EQ_INT(m_Meira.phase, MEIRA_BILLIARDS);
    Arena_Free(&f.arena);
}

TEST(meira_lethal_hit_runs_flowers_and_clears_only_after_96_defeat_updates)
{
    FIXTURE f;
    TestGameplay_FixtureInit(&f);
    GAMEPLAY *const game = TestGameplay_State();
    M_MeiraDash(&f, 1);
    const MEIRA_FLOWER *const flower = &m_Meira.flowers[0];
    while (flower->frames_left > 1) {
        M_MeiraStep(&f, 32);
    }
    TestBossFight_State()->phase_frame = 32;
    TestBossFight_State()->damage = 2399;
    p_Bomb->active = 1;
    p_Player->current =
        (ENTITY_POINT) { flower->position.x - 16, flower->position.y - 1 };
    CHECK_EQ_INT(
        M_UpdateCore(
            TestBossFight_State(), &game->assets.patterns,
            (uint8_t)f.session.rank),
        1);
    CHECK_EQ_INT(BossFight_State()->damage, 2400);
    CHECK_EQ_INT(BossFight_State()->defeated, 1);
    CHECK_EQ_INT(p_Score->delta, 30001);
    CHECK_EQ_INT(p_Player->invincibility, 200);
    CHECK_EQ_INT(p_Player->hit, 1);
    CHECK_EQ_INT(flower->frames_left, 0);
    const uint32_t world_frame = g_Stage->world.frame;
    for (int frame = 1; frame < 96; frame++) {
        M_InvalidateCore(TestBossFight_State());
        CHECK_EQ_INT(
            M_UpdateCore(
                TestBossFight_State(), &game->assets.patterns,
                (uint8_t)f.session.rank),
            1);
        CHECK_EQ_INT(m_Meira.kill_frame, frame);
    }
    p_Bomb->active = 0;
    M_MeiraDash(&f, 1);
    const uint8_t remaining = flower->frames_left;
    CHECK_EQ_INT(flower->flag, ENTITY_ACTIVE);
    CHECK(remaining > 0);
    TestBossFight_State()->defeated = 1;
    CHECK_EQ_INT(
        M_UpdateCore(
            TestBossFight_State(), &game->assets.patterns,
            (uint8_t)f.session.rank),
        2);
    CHECK_EQ_INT(m_Meira.kill_frame, 0);
    CHECK_EQ_INT(flower->frames_left, remaining);
    CHECK_EQ_INT(p_Score->delta, 30001);
    CHECK_EQ_INT(g_Stage->world.frame, world_frame);
    CHECK_EQ_INT(f.session.stage, 0);
    Arena_Free(&f.arena);
}

TEST(meira_flowers_keep_easy_parity_wrap_and_deferred_cleanup)
{
    FIXTURE f;
    TestGameplay_FixtureInit(&f);
    GAMEPLAY *const game = TestGameplay_State();
    for (unsigned expired = 0; expired < 255; expired++) {
        M_MeiraDash(&f, 1);
        while (m_Meira.flowers[0].flag == ENTITY_ACTIVE) {
            M_MeiraStep(&f, 32);
        }
        M_InvalidateCore(TestBossFight_State());
    }
    CHECK_EQ_INT(m_Meira.flower_fire_i, 255);
    M_MeiraDash(&f, 2);
    while (m_Meira.flowers[0].frame < 32) {
        M_MeiraStep(&f, 32);
    }
    CHECK_EQ_INT(m_Meira.flowers[1].frame, 29);
    Bullets_Init();
    const ENTITY_POINT origin = m_Meira.flowers[1].position;
    const uint8_t reserved = m_Meira.flowers[0].reserved;
    TestBossFight_State()->phase_frame = 32;
    CHECK_EQ_INT(
        M_UpdateCore(
            TestBossFight_State(), &game->assets.patterns,
            (uint8_t)f.session.rank),
        1);
    CHECK_EQ_INT(m_Meira.flower_fire_i, 0);
    CHECK_EQ_INT(g_Bullets->slots[0].state, ENTITY_FREE);
    while (m_Meira.flowers[1].flag == ENTITY_ACTIVE) {
        M_MeiraStep(&f, 32);
    }
    CHECK_EQ_INT(m_Meira.flower_fire_i, 1);
    CHECK_EQ_INT(g_Bullets->slots[0].state, ENTITY_ACTIVE);
    CHECK_EQ_INT(
        g_Bullets->slots[0].position[g_Stage->scroll.back].x,
        (origin.x + 12) * 16);
    CHECK_EQ_INT(g_Bullets->slots[1].state, ENTITY_FREE);
    CHECK_EQ_INT(m_Meira.flowers[0].flag, ENTITY_REMOVE);
    CHECK_EQ_INT(m_Meira.flowers[1].flag, ENTITY_REMOVE);
    M_InvalidateCore(TestBossFight_State());
    CHECK_EQ_INT(m_Meira.flowers[0].flag, ENTITY_FREE);
    CHECK_EQ_INT(m_Meira.flowers[1].flag, ENTITY_FREE);
    CHECK_EQ_INT(m_Meira.flowers[0].reserved, reserved);
    Arena_Free(&f.arena);
}

TEST(meira_trail_cleanup_requires_both_coordinates_to_differ)
{
    FIXTURE f;
    TestGameplay_FixtureInit(&f);
    TestBossFight_State()->sprite = 141;
    M_Place(TestBossFight_State());
    M_MeiraFlight(&f);
    M_MeiraStep(&f, 49);
    const ENTITY_POINT trail[3] = { { 96, 48 }, { 192, 112 }, { 96, 112 } };
    for (unsigned i = 0; i < 3; i++) {
        TestBossFight_State()->position[0] = trail[i];
        M_MeiraStep(&f, (int16_t)(55 + i * 8));
    }
    p_Stage->scroll.back = 1;
    TestBossFight_State()->position[1] = (ENTITY_POINT) { 32, 16 };
    M_MeiraStep(&f, 49);
    memset(&p_Background->cache, 0, sizeof(p_Background->cache));
    p_Stage->scroll.back = 0;
    p_Stage->scroll.front = 1;
    TestBossFight_State()->position[0] = (ENTITY_POINT) { 192, 48 };
    TestBossFight_State()->position[1] = (ENTITY_POINT) { 224, 80 };
    M_InvalidateCore(TestBossFight_State());
    CHECK_EQ_INT(p_Background->cache.dirty[4][3], 0);
    CHECK_EQ_INT(p_Background->cache.dirty[10][7], 0);
    CHECK_EQ_INT(p_Background->cache.dirty[4][7], 1);
    CHECK_EQ_INT(BossFight_State()->position[0].x, 224);
    CHECK_EQ_INT(BossFight_State()->position[0].y, 80);
    CHECK_EQ_INT(m_Meira.trail[0][0].x, 32);
    CHECK_EQ_INT(m_Meira.trail[0][0].y, 16);
    Arena_Free(&f.arena);
}
