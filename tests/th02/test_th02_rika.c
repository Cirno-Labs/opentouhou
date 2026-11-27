#include "tests/test.h"
#include "tests/th02/th02_clip_fixture.h"
#include "tests/th02/th02_palette_fixture.h"
#include "tests/th02/th02_random_fixture.h"
#include "tests/th02/th02_score_fixture.h"
#include "tests/th02/th02_sound.h"
#include "th02/background/private.h"
#include "th02/bomb/logic.c"
#include "th02/boss/fight.h"
#include "th02/boss/rika.c"
#include "th02/bullets/private.h"
#include "th02/grcg.h"
#include "th02/performance_private.h"
#include "th02/pillars.h"
#include "th02/player.h"
#include "th02/shots/fire.c"
#include "th02/shots/pool.c"
#include "th02/sparks.h"
#include "th02/stage/flow.c"
#include "th02/stage/private.h"
#include "th02/stage_effects.c"
#include "th02/video.h"

#include "core/base/arena.h"

#include <string.h>

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
    Arena_Init(&f->arena, "rika test", 2 * 1024 * 1024);
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
    Pillars_ResetStage(0);
    Bullets_Init();
    BulletSpawn_Init(1);
    Score_Init();
    Player_Init();
    Sparks_Init();
    SoundEffects_Init(nullptr);
    Boss_Init(p_BossState);
    M_InitCore();
}

TEST(rika_first_attack_emits_aimed_triple_and_wide_fan)
{
    FIXTURE f;
    M_Init(&f);
    p_Performance->value = 0;
    BOSS_STATE *const boss = p_BossState;
    M_BeginCore(boss, 1);
    boss->phase_frame = 63;
    CHECK_EQ_INT(M_UpdateCore(boss, &f.patterns, 1), BOSS_RUNNING);
    static const ENTITY_POINT velocities[] = {
        { 0, 30 }, { -13, 27 }, { 12, 27 }, { -24, 18 }, { 23, 18 },
    };
    for (size_t i = 0; i < 8; i++) {
        const BULLET *const bullet = &g_Bullets->slots[i];
        CHECK_EQ_INT(bullet->state, ENTITY_ACTIVE);
        CHECK_EQ_INT(bullet->kind, BULLET_PELLET);
        CHECK_EQ_INT(bullet->position[g_Stage->scroll.back].x, 219 * 16);
        CHECK_EQ_INT(bullet->position[g_Stage->scroll.back].y, 112 * 16);
        if (i >= 3) {
            CHECK_EQ_INT(bullet->velocity.x, velocities[i - 3].x);
            CHECK_EQ_INT(bullet->velocity.y, velocities[i - 3].y);
        }
    }
    CHECK_EQ_INT(g_Bullets->slots[8].state, ENTITY_FREE);
    Arena_Free(&f.arena);
}

TEST(rika_entry_preserves_phase_clock_and_attack_history)
{
    FIXTURE f;
    M_Init(&f);
    M_BeginCore(p_BossState, (uint8_t)f.session.rank);
    p_BossState->damage = 1400;
    p_BossState->phase_frame = 17;
    CHECK_EQ_INT(
        M_UpdateCore(p_BossState, &f.patterns, (uint8_t)f.session.rank),
        BOSS_RUNNING);
    p_BossState->damage = 700;
    p_Stage->world.frame = 32;
    CHECK_EQ_INT(
        M_UpdateCore(p_BossState, &f.patterns, (uint8_t)f.session.rank),
        BOSS_RUNNING);
    p_BossState->defeated = 1;
    for (int frame = 0; frame < 7; frame++) {
        CHECK_EQ_INT(
            M_UpdateCore(p_BossState, &f.patterns, (uint8_t)f.session.rank),
            BOSS_RUNNING);
    }
    CHECK_EQ_INT(m_Rika.current.x, 192);
    CHECK_EQ_INT(m_Rika.angle, 24);
    CHECK_EQ_INT(m_Rika.frame, 1);
    CHECK_EQ_INT(m_Rika.kill_frame, 7);
    p_BossState->phase_frame = 177;
    p_BossState->position[0].y = 123;
    p_BossState->damage = 700;
    p_BossState->defeated = 1;
    p_Stage->world.frame = 900;
    p_Stage->scroll.line = 399;
    M_BeginCore(p_BossState, (uint8_t)f.session.rank);
    CHECK_EQ_INT(BossFight_State()->phase_frame, 177);
    CHECK_EQ_INT(BossFight_State()->position[0].x, 192);
    CHECK_EQ_INT(BossFight_State()->position[1].x, 192);
    CHECK_EQ_INT(BossFight_State()->position[0].y, 123);
    CHECK_EQ_INT(BossFight_State()->damage, 0);
    CHECK_EQ_INT(BossFight_State()->defeated, 0);
    CHECK_EQ_INT(BossFight_State()->sprite, 150);
    CHECK_EQ_INT(BossFight_State()->angle, 32);
    CHECK_EQ_INT(g_Stage->world.frame, 0);
    CHECK_EQ_INT(m_Rika.current.x, 192);
    CHECK_EQ_INT(m_Rika.current.y, 47);
    CHECK_EQ_INT(m_Rika.angle, 24);
    CHECK_EQ_INT(m_Rika.frame, 1);
    CHECK_EQ_INT(m_Rika.kill_frame, 7);
    CHECK(g_Pillars->enabled);
    CHECK_EQ_INT(m_Rika.fire_mask, 7);
    f.session.rank = 1;
    M_BeginCore(p_BossState, (uint8_t)f.session.rank);
    CHECK_EQ_INT(m_Rika.fire_mask, 3);
    CHECK_EQ_INT(m_Rika.mirrored_pattern, BULLET_FAN2_MEDIUM);
    p_BossState->phase_frame = 0;
    M_MidbossUpdateCore(p_BossState, &f.patterns);
    CHECK(m_Midboss.active);
    Arena_Free(&f.arena);
}

TEST(rika_enters_pillar_attack_and_retains_source_attack_boundaries)
{
    FIXTURE f;
    M_Init(&f);
    M_BeginCore(p_BossState, (uint8_t)f.session.rank);
    p_BossState->damage = 1400;
    p_BossState->phase_frame = 17;
    CHECK_EQ_INT(
        M_UpdateCore(p_BossState, &f.patterns, (uint8_t)f.session.rank), 1);
    CHECK_EQ_INT(m_Rika.phase, RIKA_CENTER);
    CHECK_EQ_INT(m_Rika.frame, 1);
    CHECK_EQ_INT(BossFight_State()->position[g_Stage->scroll.back].x, 191);
    for (int frame = 2; frame <= 100; frame++) {
        M_InvalidateCore(p_BossState);
        CHECK_EQ_INT(
            M_UpdateCore(p_BossState, &f.patterns, (uint8_t)f.session.rank), 1);
    }
    CHECK_EQ_INT(m_Rika.phase, RIKA_HOLD);
    CHECK_EQ_INT(BossFight_State()->phase_frame, 0);
    CHECK_EQ_INT(g_Pillars->frames_new, 64);
    CHECK_EQ_INT(g_Pillars->slots[0].state, ENTITY_ACTIVE);
    CHECK_EQ_INT(g_Pillars->slots[0].frames, 64);
    CHECK_EQ_INT(g_Pillars->slots[0].duration, 160);
    CHECK_EQ_INT(g_Pillars->slots[0].position.x, m_Rika.current.x + 2);
    CHECK_EQ_INT(g_Pillars->slots[1].position.x, m_Rika.current.x + 46);
    for (int frame = 101; frame <= 280; frame++) {
        M_InvalidateCore(p_BossState);
        CHECK_EQ_INT(
            M_UpdateCore(p_BossState, &f.patterns, (uint8_t)f.session.rank), 1);
        if (frame == 260) {
            CHECK_EQ_INT(BossFight_State()->sprite, 152);
        } else if (frame == 270) {
            CHECK_EQ_INT(BossFight_State()->sprite, 153);
        }
    }
    CHECK_EQ_INT(g_Bullets->slots[0].state, ENTITY_ACTIVE);
    CHECK_EQ_INT(m_Rika.frame, 280);
    Arena_Free(&f.arena);
}

TEST(stage_one_midboss_enters_through_live_modules_and_activates_weather)
{
    FIXTURE f;
    M_Init(&f);
    p_Stage->scroll.line = 0;
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
    CHECK_EQ_INT(g_StageEffects->storm_frame, 1);
    CHECK_EQ_INT(m_Shots.target.x, 216);
    CHECK_EQ_INT(m_Shots.target.y, 64);
    StageScroll_Swap();
    for (int frame = 2; frame <= 17; frame++) {
        CHECK(BossFight_MidbossInvalidate(f.session.stage));
        BossFight_MidbossUpdate(
            &f.patterns, f.session.stage, (uint8_t)f.session.rank);
        CHECK_EQ_INT(
            BossFight_State()->position[g_Stage->scroll.back].y,
            -16 + (frame - 1) * 2);
        StageScroll_Swap();
    }
    StageEffects_Update(
        StageScroll_Pages(), g_Stage->index, g_Stage->reduce_effects,
        &p_Stage->world.slowdown);
    CHECK_EQ_INT(g_StageEffects->storm_frame, 2);
    CHECK_EQ_INT(p_Palette->colors[8][0], 176);
    m_StageEffects.storm_frame = 527;
    StageEffects_Update(
        StageScroll_Pages(), g_Stage->index, g_Stage->reduce_effects,
        &p_Stage->world.slowdown);
    CHECK_EQ_INT(g_StageEffects->storm_frame, 1000);
    CHECK_EQ_INT(g_StageEffects->lightning_frame, 1);
    CHECK_EQ_INT(g_Sparks->acceleration_x, -1);
    CHECK_EQ_INT(g_Stage->world.frame, world);
    CHECK_EQ_INT(f.session.frame, session);
    Arena_Free(&f.arena);
}

TEST(default_stage_one_boss_hits_defeats_and_finishes_through_stage_flow)
{
    FIXTURE f;
    M_Init(&f);
    f.session.frame = 713;
    Stage_Begin(f.session.stage, f.session.reduce_effects);
    StageFlow_Begin(f.session.stage, f.session.reduce_effects);
    const BOSS_ASSETS assets = { 0 };
    const BOSS_MUSIC music = { 0 };
    const TH02_INPUT input = { 0 };
    TEST_SOUND sound;
    TestSound_Init(&sound);
    p_Stage->world.frame = 713;
    p_Stage->scroll.line = 0;
    p_Stage->scroll.done = 1;
    CHECK_EQ_INT(
        StageFlow_Enter(&f.patterns, &assets, &music, &f.session, &input),
        STAGE_FLOW_BOSS_STARTED);
    CHECK_EQ_INT(g_Stage->world.frame, 0);
    CHECK(g_Pillars->enabled);
    p_BossState->damage = 2259;
    p_BossState->phase_frame = 40;
    m_Bomb.active = 1;
    const uint16_t next = g_Sparks->next;
    StageFlow_Invalidate(f.session.stage, f.session.reduce_effects);
    CHECK_EQ_INT(
        StageFlow_Update(&f.patterns, &assets, &music, &f.session, &input),
        STAGE_FLOW_RUNNING);
    CHECK_EQ_INT(g_Stage->world.progression, 1);
    CHECK_EQ_INT(BossFight_State()->damage, 2260);
    CHECK_EQ_INT(BossFight_State()->defeated, 0);
    CHECK_EQ_INT(m_Score.delta, 1);
    SoundEffects_Update();
    TestSound_Check(&sound, 4);
    CHECK_EQ_INT(
        g_Video.pages[g_Video.drawing_page][48 * VIDEO_WIDTH + 191], 15);
    p_Stage->world.frame = 2;
    StageFlow_Invalidate(f.session.stage, f.session.reduce_effects);
    CHECK_EQ_INT(
        StageFlow_Update(&f.patterns, &assets, &music, &f.session, &input),
        STAGE_FLOW_RUNNING);
    CHECK_EQ_INT(g_Stage->world.progression, 1);
    CHECK_EQ_INT(BossFight_State()->damage, 2261);
    CHECK_EQ_INT(BossFight_State()->defeated, 1);
    CHECK_EQ_INT(m_Score.delta, 20002);
    CHECK_EQ_INT(g_Player->invincibility, 200);
    CHECK_EQ_INT(m_Rika.frame, 2);
    CHECK_EQ_INT(g_Sparks->next, next);
    m_Bomb.active = 0;
    for (int frame = 1; frame <= 80; frame++) {
        StageFlow_Invalidate(f.session.stage, f.session.reduce_effects);
        CHECK_EQ_INT(
            StageFlow_Update(&f.patterns, &assets, &music, &f.session, &input),
            STAGE_FLOW_RUNNING);
        CHECK_EQ_INT(g_Stage->world.progression, frame == 80 ? 2 : 1);
        CHECK_EQ_INT(m_Rika.kill_frame, frame == 80 ? 0 : frame);
    }
    CHECK_EQ_INT(g_Sparks->next, (next + 160) % SPARK_COUNT);
    CHECK_EQ_INT(m_Score.delta, 20002);
    CHECK_EQ_INT(f.session.frame, 713);
    CHECK_EQ_INT(g_Stage->world.frame, 2);
    p_Stage->world.progression = 2;
    CHECK_EQ_INT(
        StageFlow_End(&f.patterns, &assets, &music, &f.session, &input),
        STAGE_FLOW_CLEARED);
    CHECK_EQ_INT(g_Stage->world.progression, 2);
    CHECK_EQ_INT(f.session.stage, 0);
    TestSound_Free(&sound);
    Arena_Free(&f.arena);
}
