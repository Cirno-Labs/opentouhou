#include "th02/background/private.h"
#include "th02/boss/fight.h"
#include "th02/bullets/private.h"
#include "th02/stage/private.h"
#include "th02_score_fixture.h"
#include "th02_random_fixture.h"
#include "th02_clip_fixture.h"
#include "th02_palette_fixture.h"
#include "../../src/th02/performance.c"
#include "../../src/th02/shots/pool.c"
#include "../../src/th02/shots/fire.c"
#include "../../src/th02/bomb/logic.c"
#include "../../src/th02/player/logic.c"
#include "../../src/th02/player/render.c"
#include "../../src/th02/miss.c"
#include "th02/boss/meira.c"
#include "th02/grcg.h"
#include "th02/pillars.h"
#include "th02/sparks.h"
#include "th02/stage_effects.h"
#include "th02/video.h"

#include "core/base/arena.h"
#include "test.h"
#include "th02_sound.h"

typedef struct {
    ARENA arena;
    PATTERN_BANK patterns;
    TH02_SESSION session;
} FIXTURE;

static void M_Init(FIXTURE *const f)
{
    memset(f, 0, sizeof(*f));
    Random_Reset();
    Clip_Reset();
    Palette_Init();
    Arena_Init(&f->arena, "meira test", 2 * 1024 * 1024);
    const ALLOCATOR *const allocator = Arena_Allocator(&f->arena);
    Video_Init(allocator);
    memset(g_Video.pages[0], 0, VIDEO_PAGE_SIZE);
    memset(g_Video.pages[1], 0, VIDEO_PAGE_SIZE);
    Grcg_Init();
    PatternBank_Init(&f->patterns, allocator);
    uint8_t body[32 * 48];
    memset(body, 5, sizeof(body));
    for (uint16_t slot = 0; slot < 48; slot++) {
        CHECK(Result_Ignore(
            PatternBank_Put(&f->patterns, slot, 32, 48, body, 0)));
    }
    uint8_t tiny[256];
    memset(tiny, 9, sizeof(tiny));
    for (uint16_t slot = 48; slot < 128; slot++) {
        CHECK(Result_Ignore(
            PatternBank_Put(&f->patterns, slot, 16, 16, tiny, 0)));
        CHECK(Result_Ignore(PatternBank_ConvertTiny(&f->patterns, slot)));
    }
    uint8_t boss[64 * 96];
    memset(boss, 6, sizeof(boss));
    for (uint16_t slot = 128; slot < 164; slot++) {
        CHECK(Result_Ignore(PatternBank_Put(
            &f->patterns, slot, 64, slot < 148 ? 64 : 96, boss, 0)));
    }
    Stage_Init();
    Background_Init();
    StageEffects_Init();
    Pillars_Init();
    Pillars_ResetStage(1);
    Bullets_Init();
    BulletSpawn_Init(0);
    Score_Init();
    Shots_Init();
    Shots_ResetTarget();
    Player_Init();
    Bomb_Init();
    Miss_Init();
    Sparks_Init();
    SoundEffects_Init(nullptr);
    Boss_Init(p_BossState);
    Meira_Init();
}

static void M_MeiraStep(FIXTURE *const f, const int16_t frame)
{
    p_BossState->phase_frame = frame;
    CHECK_EQ_INT(
        Meira_Update(p_BossState, &f->patterns, (uint8_t)f->session.rank),
        BOSS_RUNNING);
}

static void M_MeiraDash(FIXTURE *const f, const unsigned flowers)
{
    p_BossState->sprite = 141;
    Meira_Place(p_BossState);
    Meira_Begin(p_BossState, (uint8_t)f->session.rank);
    m_Player.current = (ENTITY_POINT) { 0, 336 };
    M_MeiraStep(f, 90);
    M_MeiraStep(f, 19);
    for (unsigned i = 0; i < flowers; i++) {
        M_MeiraStep(f, (int16_t)(20 + i));
    }
}

static void M_MeiraFlight(FIXTURE *const f)
{
    Meira_Begin(p_BossState, (uint8_t)f->session.rank);
    p_BossState->damage = 701;
    M_MeiraStep(f, 90);
    p_BossState->position[g_Stage->scroll.back] = (ENTITY_POINT) { 16, 48 };
    M_MeiraStep(f, 80);
    p_BossState->damage = 1501;
    M_MeiraStep(f, 119);
    M_MeiraStep(f, 98);
    CHECK_EQ_INT(m_Meira.phase, MEIRA_BILLIARDS);
    CHECK_EQ_INT(m_Meira.pattern, 1);
    p_BossState->damage = 0;
}

TEST(stage_two_midboss_enters_through_modules_and_activates_lightning)
{
    FIXTURE f;
    M_Init(&f);
    f.session.stage = 1;
    Stage_Begin(1, f.session.reduce_effects);
    p_BossState->position[0].y = 41;
    p_BossState->defeated = 7;
    p_BossState->phase_frame = 320;
    p_BossState->position[0] = p_BossState->position[1] =
        (ENTITY_POINT) { 192, 64 };
    for (int frame = 0; frame < 13; frame++) {
        Meira_MidbossUpdate(p_BossState, &f.patterns);
    }
    p_BossState->phase_frame = 0;
    p_BossState->position[0].y = 41;
    p_Stage->world.midboss_active = 1;
    const uint32_t world = g_Stage->world.frame;
    const int32_t session = f.session.frame;
    BossFight_MidbossUpdate(
        &f.patterns, f.session.stage, (uint8_t)f.session.rank);
    CHECK(m_Midboss.active);
    CHECK_EQ_INT(BossFight_State()->phase_frame, 1);
    CHECK_EQ_INT(BossFight_State()->position[0].x, 192);
    CHECK_EQ_INT(BossFight_State()->position[1].x, 192);
    CHECK_EQ_INT(BossFight_State()->position[0].y, -16);
    CHECK_EQ_INT(BossFight_State()->position[1].y, -16);
    CHECK_EQ_INT(BossFight_State()->defeated, 7);
    CHECK_EQ_INT(m_Midboss.kill_frame, 13);
    CHECK_EQ_INT(m_Shots.target.x, 216);
    CHECK_EQ_INT(m_Shots.target.y, 65);
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
    M_Init(&f);
    p_BossState->position[0] = p_BossState->position[1] =
        (ENTITY_POINT) { 192, 64 };
    p_BossState->phase_frame = 100;
    p_BossState->damage = 70;
    p_Stage->world.frame = 2;
    m_Shots.weapon.level = 0;
    SHOT *const shot = &m_Shots.slots[0];
    shot->state = ENTITY_ACTIVE;
    shot->sprite = 50;
    shot->position[p_Stage->scroll.back] = (ENTITY_POINT) { 208 * 16, 80 * 16 };
    Meira_MidbossUpdate(p_BossState, &f.patterns);
    CHECK_EQ_INT(BossFight_State()->damage, 71);
    CHECK_EQ_INT(m_Score.delta, 6);
    CHECK_EQ_INT(shot->age, 2);
    CHECK_EQ_INT(g_Sparks->next, 1);
    Meira_MidbossUpdate(p_BossState, &f.patterns);
    CHECK_EQ_INT(BossFight_State()->damage, 71);
    CHECK_EQ_INT(m_Score.delta, 6);
    p_BossState->phase_frame = 319;
    shot->age = 0;
    Meira_MidbossUpdate(p_BossState, &f.patterns);
    CHECK_EQ_INT(BossFight_State()->phase_frame, 320);
    CHECK_EQ_INT(BossFight_State()->damage, 77);
    CHECK_EQ_INT(m_Score.delta, 12);
    CHECK_EQ_INT(shot->age, 2);
    CHECK_EQ_INT(g_Sparks->next, 2);
    Arena_Free(&f.arena);
}

TEST(stage_two_midboss_defeat_clears_shared_flag_after_three_sprite_bursts)
{
    FIXTURE f;
    M_Init(&f);
    f.session.stage = 1;
    p_BossState->position[0] = p_BossState->position[1] =
        (ENTITY_POINT) { 192, 64 };
    Meira_MidbossUpdate(p_BossState, &f.patterns);
    p_BossState->position[0] = p_BossState->position[1] =
        (ENTITY_POINT) { 192, 64 };
    p_BossState->phase_frame = 320;
    p_BossState->damage = 380;
    p_Stage->scroll.line = 240;
    m_Bomb.active = 1;
    p_Stage->world.midboss_active = 1;
    m_Player.invincibility = 17;
    Meira_MidbossUpdate(p_BossState, &f.patterns);
    CHECK_EQ_INT(BossFight_State()->damage, 381);
    CHECK_EQ_INT(BossFight_State()->defeated, 0);
    CHECK_EQ_INT(m_Score.delta, 1);
    p_Stage->scroll.line = 239;
    Meira_MidbossUpdate(p_BossState, &f.patterns);
    CHECK_EQ_INT(BossFight_State()->damage, 382);
    CHECK_EQ_INT(BossFight_State()->defeated, 1);
    CHECK_EQ_INT(m_Score.delta, 20002);
    const uint16_t next = g_Sparks->next;
    unsigned expected = 0;
    for (int frame = 1; frame <= 64; frame++) {
        CHECK(Meira_MidbossInvalidate(p_BossState));
        Meira_MidbossUpdate(p_BossState, &f.patterns);
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
    CHECK_EQ_INT(m_Shots.target.x, -1);
    CHECK_EQ_INT(m_Shots.target.y, -1);
    CHECK_EQ_INT(m_Player.invincibility, 17);
    CHECK_EQ_INT(m_Score.delta, 20002);
    CHECK_EQ_INT(g_Stage->world.midboss_active, 1);
    CHECK(!BossFight_MidbossInvalidate(f.session.stage));
    Arena_Free(&f.arena);
}

TEST(stage_two_midboss_chase_bullets_read_live_performance_page_and_bomb)
{
    FIXTURE f;
    M_Init(&f);
    f.session.rank = 1;
    BulletSpawn_Init(1);
    p_BossState->position[0] = p_BossState->position[1] =
        (ENTITY_POINT) { 192, 64 };
    p_BossState->phase_frame = 383;
    m_Performance.value = 48;
    const RANDOM_STATE before_random = m_Rng;
    Meira_MidbossUpdate(p_BossState, &f.patterns);
    CHECK_EQ_INT(g_Bullets->slots[0].state, ENTITY_ACTIVE);
    CHECK_EQ_INT(g_Bullets->slots[0].motion, BULLET_MOTION_CHASE);
    CHECK_EQ_INT(g_Bullets->slots[0].sprite, 84);
    CHECK_EQ_INT(g_Bullets->slots[0].speed, 60);
    CHECK_EQ_INT(g_Bullets->slots[0].position[0].x, 220 * 16);
    CHECK_EQ_INT(g_Bullets->slots[0].position[0].y, 96 * 16);
    p_BossState->phase_frame = 391;
    m_Performance.value = 24;
    p_Stage->scroll.back = 1;
    Meira_MidbossUpdate(p_BossState, &f.patterns);
    CHECK_EQ_INT(g_Bullets->slots[1].state, ENTITY_ACTIVE);
    CHECK_EQ_INT(g_Bullets->slots[1].speed, 45);
    CHECK_EQ_INT(g_Bullets->slots[1].position[1].x, 220 * 16);
    p_BossState->phase_frame = 399;
    m_Bomb.active = 1;
    Meira_MidbossUpdate(p_BossState, &f.patterns);
    CHECK_EQ_INT(g_Bullets->slots[2].state, ENTITY_FREE);
    CHECK(memcmp(&m_Rng, &before_random, sizeof(before_random)) == 0);
    Arena_Free(&f.arena);
}

TEST(stage_two_midboss_timeout_waits_for_no_hit_without_award_or_sound)
{
    FIXTURE f;
    M_Init(&f);
    TEST_SOUND sound;
    TestSound_Init(&sound);
    p_BossState->position[0] = p_BossState->position[1] =
        (ENTITY_POINT) { 192, 64 };
    p_BossState->phase_frame = 1600;
    p_BossState->damage = 380;
    p_Stage->scroll.line = 240;
    m_Bomb.active = 1;
    p_Stage->world.frame = 2;
    Meira_MidbossUpdate(p_BossState, &f.patterns);
    CHECK_EQ_INT(BossFight_State()->defeated, 0);
    CHECK_EQ_INT(BossFight_State()->damage, 381);
    CHECK_EQ_INT(m_Score.delta, 1);
    SoundEffects_Update();
    TestSound_Check(&sound, 4);
    p_Stage->world.frame = 3;
    Meira_MidbossUpdate(p_BossState, &f.patterns);
    CHECK_EQ_INT(BossFight_State()->defeated, 1);
    CHECK_EQ_INT(m_Score.delta, 1);
    CHECK_EQ_INT(m_Midboss.kill_frame, 0);
    SoundEffects_Update();
    TestSound_Check(&sound, -1);
    TestSound_Free(&sound);
    Arena_Free(&f.arena);
}

TEST(meira_entry_preserves_clocks_flowers_and_attack_history)
{
    FIXTURE f;
    M_Init(&f);
    f.session.stage = 1;
    p_Stage->world.frame = 900;
    p_BossState->phase_frame = 77;
    M_MeiraDash(&f, 2);
    M_MeiraFlight(&f);
    M_MeiraStep(&f, 49);
    p_BossState->defeated = 1;
    for (int frame = 0; frame < 9; frame++) {
        CHECK_EQ_INT(
            Meira_Update(p_BossState, &f.patterns, (uint8_t)f.session.rank),
            BOSS_RUNNING);
    }
    const MEIRA before = m_Meira;
    CHECK_EQ_INT(before.kill_frame, 9);
    CHECK(before.trail_active);
    CHECK_EQ_INT(before.flowers[0].flag, ENTITY_ACTIVE);
    p_BossState->phase_frame = 77;
    p_BossState->damage = 999;
    p_BossState->defeated = 1;
    p_BossState->sprite = 141;
    Meira_Place(p_BossState);
    Meira_Begin(p_BossState, (uint8_t)f.session.rank);
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
    Meira_Begin(p_BossState, (uint8_t)f.session.rank);
    CHECK_EQ_INT(m_Meira.fan_pattern, BULLET_FAN5_WIDE);
    CHECK_EQ_INT(m_Meira.burst_pattern, BULLET_FAN5_MEDIUM_AIMED);
    p_BossState->phase_frame = 0;
    Meira_MidbossUpdate(p_BossState, &f.patterns);
    CHECK(m_Midboss.active);
    Meira_Init();
    BossFight_Init();
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
    M_Init(&f);
    p_BossState->sprite = 141;
    Meira_Place(p_BossState);
    Meira_Begin(p_BossState, (uint8_t)f.session.rank);
    p_BossState->damage = 701;
    p_BossState->phase_frame = 90;
    CHECK_EQ_INT(
        Meira_Update(p_BossState, &f.patterns, (uint8_t)f.session.rank), 1);
    CHECK_EQ_INT(BossFight_State()->phase_frame, 0);
    CHECK_EQ_INT(m_Meira.pattern, 1);
    CHECK_EQ_INT(m_Meira.phase, MEIRA_OPENING);
    p_BossState->damage = 700;
    p_BossState->position[g_Stage->scroll.back] = (ENTITY_POINT) { 16, 48 };
    M_MeiraStep(&f, 80);
    CHECK_EQ_INT(m_Meira.pattern, 2);
    p_BossState->position[p_Stage->scroll.back].x = 192;
    M_MeiraStep(&f, 155);
    CHECK_EQ_INT(m_Meira.pattern, 3);
    p_BossState->damage = 701;
    p_BossState->phase_frame = 115;
    CHECK_EQ_INT(
        Meira_Update(p_BossState, &f.patterns, (uint8_t)f.session.rank), 1);
    CHECK_EQ_INT(BossFight_State()->phase_frame, 0);
    CHECK_EQ_INT(m_Meira.pattern, 0);
    CHECK_EQ_INT(m_Meira.phase, MEIRA_PRESSURE);
    p_BossState->damage = 1501;
    p_BossState->phase_frame = 118;
    CHECK_EQ_INT(
        Meira_Update(p_BossState, &f.patterns, (uint8_t)f.session.rank), 1);
    CHECK_EQ_INT(m_Meira.phase, MEIRA_PRESSURE);
    CHECK_EQ_INT(
        Meira_Update(p_BossState, &f.patterns, (uint8_t)f.session.rank), 1);
    CHECK_EQ_INT(BossFight_State()->phase_frame, 0);
    CHECK_EQ_INT(m_Meira.pattern, 0);
    CHECK_EQ_INT(m_Meira.phase, MEIRA_BILLIARDS);
    Arena_Free(&f.arena);
}

TEST(meira_lethal_hit_runs_flowers_and_clears_only_after_96_defeat_updates)
{
    FIXTURE f;
    M_Init(&f);
    M_MeiraDash(&f, 1);
    const MEIRA_FLOWER *const flower = &m_Meira.flowers[0];
    while (flower->frames_left > 1) {
        M_MeiraStep(&f, 32);
    }
    p_BossState->phase_frame = 32;
    p_BossState->damage = 2399;
    m_Bomb.active = 1;
    m_Player.current =
        (ENTITY_POINT) { flower->position.x - 16, flower->position.y - 1 };
    CHECK_EQ_INT(
        Meira_Update(p_BossState, &f.patterns, (uint8_t)f.session.rank), 1);
    CHECK_EQ_INT(BossFight_State()->damage, 2400);
    CHECK_EQ_INT(BossFight_State()->defeated, 1);
    CHECK_EQ_INT(m_Score.delta, 30001);
    CHECK_EQ_INT(m_Player.invincibility, 200);
    CHECK_EQ_INT(m_Player.hit, 1);
    CHECK_EQ_INT(flower->frames_left, 0);
    const uint32_t world_frame = g_Stage->world.frame;
    for (int frame = 1; frame < 96; frame++) {
        Meira_Invalidate(p_BossState);
        CHECK_EQ_INT(
            Meira_Update(p_BossState, &f.patterns, (uint8_t)f.session.rank), 1);
        CHECK_EQ_INT(m_Meira.kill_frame, frame);
    }
    m_Bomb.active = 0;
    M_MeiraDash(&f, 1);
    const uint8_t remaining = flower->frames_left;
    CHECK_EQ_INT(flower->flag, ENTITY_ACTIVE);
    CHECK(remaining > 0);
    p_BossState->defeated = 1;
    CHECK_EQ_INT(
        Meira_Update(p_BossState, &f.patterns, (uint8_t)f.session.rank), 2);
    CHECK_EQ_INT(m_Meira.kill_frame, 0);
    CHECK_EQ_INT(flower->frames_left, remaining);
    CHECK_EQ_INT(m_Score.delta, 30001);
    CHECK_EQ_INT(g_Stage->world.frame, world_frame);
    CHECK_EQ_INT(f.session.stage, 0);
    Arena_Free(&f.arena);
}

TEST(meira_flowers_keep_easy_parity_wrap_and_deferred_cleanup)
{
    FIXTURE f;
    M_Init(&f);
    for (unsigned expired = 0; expired < 255; expired++) {
        M_MeiraDash(&f, 1);
        while (m_Meira.flowers[0].flag == ENTITY_ACTIVE) {
            M_MeiraStep(&f, 32);
        }
        Meira_Invalidate(p_BossState);
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
    p_BossState->phase_frame = 32;
    CHECK_EQ_INT(
        Meira_Update(p_BossState, &f.patterns, (uint8_t)f.session.rank), 1);
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
    Meira_Invalidate(p_BossState);
    CHECK_EQ_INT(m_Meira.flowers[0].flag, ENTITY_FREE);
    CHECK_EQ_INT(m_Meira.flowers[1].flag, ENTITY_FREE);
    CHECK_EQ_INT(m_Meira.flowers[0].reserved, reserved);
    Arena_Free(&f.arena);
}

TEST(meira_trail_cleanup_requires_both_coordinates_to_differ)
{
    FIXTURE f;
    M_Init(&f);
    p_BossState->sprite = 141;
    Meira_Place(p_BossState);
    M_MeiraFlight(&f);
    M_MeiraStep(&f, 49);
    const ENTITY_POINT trail[3] = { { 96, 48 }, { 192, 112 }, { 96, 112 } };
    for (unsigned i = 0; i < 3; i++) {
        p_BossState->position[0] = trail[i];
        M_MeiraStep(&f, (int16_t)(55 + i * 8));
    }
    p_Stage->scroll.back = 1;
    p_BossState->position[1] = (ENTITY_POINT) { 32, 16 };
    M_MeiraStep(&f, 49);
    memset(&p_Background->cache, 0, sizeof(p_Background->cache));
    p_Stage->scroll.back = 0;
    p_Stage->scroll.front = 1;
    p_BossState->position[0] = (ENTITY_POINT) { 192, 48 };
    p_BossState->position[1] = (ENTITY_POINT) { 224, 80 };
    Meira_Invalidate(p_BossState);
    CHECK_EQ_INT(p_Background->cache.dirty[4][3], 0);
    CHECK_EQ_INT(p_Background->cache.dirty[10][7], 0);
    CHECK_EQ_INT(p_Background->cache.dirty[4][7], 1);
    CHECK_EQ_INT(BossFight_State()->position[0].x, 224);
    CHECK_EQ_INT(BossFight_State()->position[0].y, 80);
    CHECK_EQ_INT(m_Meira.trail[0][0].x, 32);
    CHECK_EQ_INT(m_Meira.trail[0][0].y, 16);
    Arena_Free(&f.arena);
}
