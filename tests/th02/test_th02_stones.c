#include "tests/test.h"
#include "tests/th02/th02_clip_fixture.h"
#include "tests/th02/th02_palette_fixture.h"
#include "tests/th02/th02_random_fixture.h"
#include "tests/th02/th02_score_fixture.h"
#include "th02/background/private.h"
#include "th02/bomb/private.h"
#include "th02/boss/fight.h"
#include "th02/boss/stones.c"
#include "th02/bullets/private.h"
#include "th02/enemies_private.h"
#include "th02/grcg.h"
#include "th02/items.h"
#include "th02/miss_private.h"
#include "th02/pillars.h"
#include "th02/player/private.h"
#include "th02/shots/private.h"
#include "th02/sparks.h"
#include "th02/stage/private.h"
#include "th02/stage_effects.h"
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
    Arena_Init(&f->arena, "stones test", 2 * 1024 * 1024);
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
    Pillars_ResetStage(2);
    Bullets_Init();
    BulletSpawn_Init(0);
    Score_Init();
    Shots_Init();
    Shots_ResetTarget();
    Player_Init();
    Bomb_Init();
    Miss_Init();
    Enemies_Init();
    Items_Init();
    Sparks_Init();
    SoundEffects_Init(nullptr);
    Boss_Init(p_BossState);
    M_InitCore();
}

TEST(stage_three_midboss_keeps_shared_arrays_and_entrance_hit_ownership)
{
    FIXTURE f;
    M_Init(&f);
    f.session.stage = 2;
    m_Stones.damage[4] = 93;
    m_Stones.flag[4] = 4;
    m_Stones.sprite[4] = 160;
    m_Stones.midboss_position[0][p_Stage->scroll.back] =
        (ENTITY_POINT) { 7, 9 };

    BossFight_MidbossUpdate(
        &f.patterns, f.session.stage, (uint8_t)f.session.rank);
    CHECK_EQ_INT(BossFight_State()->phase_frame, 1);
    CHECK_EQ_INT(p_Shots->target.x, 31);
    CHECK_EQ_INT(p_Shots->target.y, 25);
    CHECK_EQ_INT(m_Stones.midboss_position[0][0].x, 32);
    CHECK_EQ_INT(m_Stones.midboss_position[1][1].x, 352);
    CHECK_EQ_INT(m_Stones.midboss_position[1][0].y, -16);
    p_Stage->scroll.line = 200;
    p_Stage->scroll.moved = 1;
    p_Bomb->active = 1;
    p_Stage->world.frame = 0;
    m_Stones.damage[0] = 280;
    m_Stones.damage[1] = 280;
    M_MidbossUpdateCore(p_BossState, &f.patterns, (uint8_t)f.session.rank);
    CHECK_EQ_INT(m_Stones.damage[0], 281);
    CHECK_EQ_INT(m_Stones.damage[1], 281);
    CHECK_EQ_INT(p_Score->delta, 2);
    CHECK_EQ_INT(m_Stones.flag[0], 0);
    CHECK_EQ_INT(m_Stones.flag[1], 0);
    CHECK_EQ_INT(m_Stones.midboss_position[0][g_Stage->scroll.back].y, -15);
    CHECK_EQ_INT(m_Stones.damage[4], 93);
    CHECK_EQ_INT(m_Stones.flag[4], 4);
    CHECK_EQ_INT(m_Stones.sprite[4], 160);
    p_BossState->phase_frame = 127;
    BossFight_MidbossUpdate(
        &f.patterns, f.session.stage, (uint8_t)f.session.rank);
    CHECK_EQ_INT(m_Stones.flag[0], 1);
    CHECK_EQ_INT(m_Stones.flag[1], 1);
    CHECK_EQ_INT(p_Score->delta, 40004);
    CHECK_EQ_INT(m_Stones.midboss_rounds, 1);
    const uint32_t frame = g_Stage->world.frame;
    const int32_t session_frame = f.session.frame;
    for (int i = 0; i < 48; i++) {
        CHECK(M_MidbossInvalidateCore());
        M_MidbossUpdateCore(p_BossState, &f.patterns, (uint8_t)f.session.rank);
    }
    CHECK_EQ_INT(m_Stones.flag[0], 2);
    CHECK_EQ_INT(m_Stones.flag[1], 2);
    CHECK_EQ_INT(m_Stones.midboss_kill_frame[0], 0);
    CHECK_EQ_INT(p_Shots->target.x, -1);
    CHECK_EQ_INT(p_Shots->target.y, -1);
    CHECK(!M_MidbossInvalidateCore());
    CHECK_EQ_INT(g_Stage->world.frame, frame);
    CHECK_EQ_INT(f.session.frame, session_frame);
    Arena_Free(&f.arena);
}

TEST(stage_three_midboss_clock_reset_changes_the_second_actor_in_call_order)
{
    FIXTURE f;
    M_Init(&f);
    f.session.stage = 2;
    p_Stage->scroll.back = 1;
    p_Stage->scroll.front = 0;
    p_Stage->scroll.line = 0;
    p_BossState->phase_frame = 575;
    m_Stones.midboss_position[0][1] = (ENTITY_POINT) { 32, 100 };
    m_Stones.midboss_position[1][1] = (ENTITY_POINT) { 352, 100 };
    M_MidbossUpdateCore(p_BossState, &f.patterns, (uint8_t)f.session.rank);
    CHECK_EQ_INT(BossFight_State()->phase_frame, 127);
    CHECK_EQ_INT(m_Stones.midboss_position[0][1].y, 100);
    CHECK_EQ_INT(m_Stones.midboss_position[1][1].y, 100);
    CHECK_EQ_INT(g_Bullets->slots[0].state, ENTITY_FREE);
    m_Stones.midboss_rounds = 3;
    M_MidbossUpdateCore(p_BossState, &f.patterns, (uint8_t)f.session.rank);
    CHECK_EQ_INT(m_Stones.midboss_rounds, 4);
    CHECK_EQ_INT(m_Stones.flag[0], 1);
    CHECK_EQ_INT(m_Stones.flag[1], 1);
    CHECK_EQ_INT(p_Score->delta, 0);
    CHECK_EQ_INT(g_Bullets->slots[0].state, ENTITY_ACTIVE);
    CHECK_EQ_INT(g_Bullets->slots[0].angle, 8);
    CHECK_EQ_INT(g_Bullets->slots[0].position[1].x, 60 * 16);
    CHECK_EQ_INT(g_Bullets->slots[1].angle, 120);
    CHECK_EQ_INT(g_Bullets->slots[1].position[1].x, 380 * 16);
    Arena_Free(&f.arena);
}

TEST(stage_three_midboss_uses_real_shot_aging_and_raw_damage)
{
    FIXTURE f;
    M_Init(&f);
    f.session.stage = 2;
    p_Stage->scroll.back = 0;
    p_Stage->scroll.front = 1;
    p_Stage->scroll.line = 0;
    p_BossState->phase_frame = 128;
    m_Stones.midboss_position[0][0] = (ENTITY_POINT) { 32, 100 };
    m_Stones.midboss_position[1][0] = (ENTITY_POINT) { 352, 100 };
    p_Shots->weapon.level = 3;
    SHOT *const shot = &p_Shots->slots[0];
    shot->state = ENTITY_ACTIVE;
    shot->position[0] = (ENTITY_POINT) { 48 * 16, 110 * 16 };
    shot->sprite = 50;
    shot->velocity.y = -128;
    M_MidbossUpdateCore(p_BossState, &f.patterns, (uint8_t)f.session.rank);
    CHECK_EQ_INT(m_Stones.damage[0], 5);
    CHECK_EQ_INT(m_Stones.damage[1], 0);
    CHECK_EQ_INT(p_Score->delta, 5);
    CHECK_EQ_INT(shot->age, 2);
    CHECK_EQ_INT(shot->velocity.y, -16);
    M_MidbossUpdateCore(p_BossState, &f.patterns, (uint8_t)f.session.rank);
    CHECK_EQ_INT(m_Stones.damage[0], 5);
    CHECK_EQ_INT(p_Score->delta, 5);
    Arena_Free(&f.arena);
}

TEST(stones_begin_resets_shared_midboss_arrays_and_keeps_retained_history)
{
    FIXTURE f;
    M_Init(&f);
    f.session.stage = 2;
    p_Stage->scroll.line = 350;
    p_Stage->world.frame = 12345;
    m_Stones.midboss_rounds = 4;
    m_Stones.midboss_position[0][0] = (ENTITY_POINT) { 32, 100 };
    m_Stones.kill_frame[4] = 17;
    m_Stones.ring_flag[5] = 3;
    m_Stones.sweep_angle = 80;
    m_Stones.flag[0] = 2;
    m_Stones.damage[0] = 290;
    p_Enemies->slots[0].state = ENTITY_ACTIVE;
    const int32_t session_frame = f.session.frame;
    M_BeginCore(p_BossState, (uint8_t)f.session.rank);
    CHECK_EQ_INT(g_Stage->world.frame, 12345);
    CHECK_EQ_INT(f.session.frame, session_frame);
    CHECK_EQ_INT(BossFight_State()->phase_frame, 0);
    CHECK_EQ_INT(m_Stones.flag[0], STONE_DORMANT);
    CHECK_EQ_INT(m_Stones.damage[0], 0);
    CHECK_EQ_INT(m_Stones.sprite[4], 148);
    CHECK_EQ_INT(m_Stones.position[0].x, 128);
    CHECK_EQ_INT(m_Stones.position[3].y, 48);
    CHECK_EQ_INT(m_Stones.position[4].x, 208);
    CHECK_EQ_INT(m_Stones.bullet.x, 220);
    CHECK_EQ_INT(m_Stones.bullet.y, 40);
    CHECK_EQ_INT(m_Stones.ring_y, 46);
    CHECK_EQ_INT(m_Stones.midboss_rounds, 4);
    CHECK_EQ_INT(m_Stones.kill_frame[4], 17);
    CHECK_EQ_INT(m_Stones.ring_flag[5], 3);
    CHECK_EQ_INT(m_Stones.sweep_angle, 80);
    CHECK_EQ_INT(p_Enemies->slots[0].state, ENTITY_REMOVE);
    CHECK(g_Pillars->enabled);
    CHECK_EQ_INT(g_Pillars->frames_new, 12);
    CHECK_EQ_INT(m_Stones.spread, BULLET_FAN3_WIDE_AIMED);
    f.session.rank = 1;
    M_BeginCore(p_BossState, (uint8_t)f.session.rank);
    CHECK_EQ_INT(m_Stones.spread, BULLET_FAN5_WIDE_AIMED);
    CHECK_EQ_INT(m_Stones.ring, BULLET_RING8);
    M_InitCore();
    BossFight_Init();
    CHECK_EQ_INT(m_Stones.midboss_rounds, 0);
    CHECK_EQ_INT(m_Stones.kill_frame[4], 0);
    CHECK_EQ_INT(m_Stones.ring_flag[5], 0);
    CHECK_EQ_INT(m_Stones.sweep_angle, 0);
    Arena_Free(&f.arena);
}

TEST(dormant_stones_consume_real_shots_without_accumulating_damage)
{
    FIXTURE f;
    M_Init(&f);
    f.session.stage = 2;
    M_BeginCore(p_BossState, (uint8_t)f.session.rank);
    p_Stage->scroll.line = 0;
    p_Stage->scroll.back = 0;
    p_Stage->scroll.front = 1;
    SHOT *const shot = &p_Shots->slots[0];
    shot->state = ENTITY_ACTIVE;
    shot->position[0] = (ENTITY_POINT) { 144 * 16, 40 * 16 };
    shot->sprite = 50;
    shot->velocity.y = -128;
    p_Shots->weapon.level = 3;
    CHECK_EQ_INT(
        M_UpdateCore(p_BossState, &f.patterns, (uint8_t)f.session.rank), 1);
    CHECK_EQ_INT(p_Score->delta, 5);
    CHECK_EQ_INT(shot->age, 2);
    CHECK_EQ_INT(shot->velocity.y, -16);
    CHECK_EQ_INT(m_Stones.damage[0], 0);
    for (int frame = 1; frame < 64; frame++) {
        M_InvalidateCore();
        CHECK_EQ_INT(
            M_UpdateCore(p_BossState, &f.patterns, (uint8_t)f.session.rank), 1);
    }
    CHECK_EQ_INT(m_Stones.flag[0], STONE_ACTIVE);
    CHECK_EQ_INT(m_Stones.flag[1], STONE_ACTIVE);
    CHECK_EQ_INT(m_Stones.sprite[0], 155);
    CHECK_EQ_INT(m_Stones.phase, STONES_INNER);
    CHECK_EQ_INT(BossFight_State()->phase_frame, 0);
    CHECK_EQ_INT(m_Stones.pattern_frame, 0);
    Arena_Free(&f.arena);
}

TEST(stones_keep_individual_item_rewards_and_deferred_final_cleanup)
{
    FIXTURE f;
    M_Init(&f);
    f.session.stage = 2;
    M_BeginCore(p_BossState, (uint8_t)f.session.rank);
    p_Stage->scroll.line = 0;
    m_Stones.phase = STONES_NORTH_FINAL;
    m_Stones.pattern = 2;
    p_Bomb->active = 1;
    p_Stage->world.frame = 0;
    for (int i = 0; i < STONE_COUNT; i++) {
        m_Stones.flag[i] = STONE_ACTIVE;
        m_Stones.damage[i] = i == STONE_NORTH ? 929 : i < 2 ? 139 : 119;
    }
    CHECK_EQ_INT(
        M_UpdateCore(p_BossState, &f.patterns, (uint8_t)f.session.rank), 1);
    CHECK_EQ_INT(p_Score->delta, 150005);
    CHECK_EQ_INT(p_Player->invincibility, 200);
    CHECK_EQ_INT(g_Items->slots[0].kind, ITEM_BOMB);
    CHECK_EQ_INT(g_Items->slots[1].kind, ITEM_BIG_POWER);
    CHECK_EQ_INT(g_Items->slots[2].kind, ITEM_BIG_POWER);
    CHECK_EQ_INT(g_Items->slots[3].kind, ITEM_BOMB);
    CHECK_EQ_INT(g_Items->slots[4].state, ENTITY_FREE);
    for (int frame = 1; frame < 96; frame++) {
        M_InvalidateCore();
        CHECK_EQ_INT(
            M_UpdateCore(p_BossState, &f.patterns, (uint8_t)f.session.rank), 1);
    }
    CHECK_EQ_INT(m_Stones.flag[4], STONE_REMOVE);
    CHECK_EQ_INT(m_Stones.kill_frame[4], 0);
    CHECK_EQ_INT(BossFight_State()->angle, 224);
    CHECK_EQ_INT(
        M_UpdateCore(p_BossState, &f.patterns, (uint8_t)f.session.rank), 1);
    M_InvalidateCore();
    CHECK_EQ_INT(
        M_UpdateCore(p_BossState, &f.patterns, (uint8_t)f.session.rank), 2);
    CHECK_EQ_INT(p_Score->delta, 150005);
    CHECK_EQ_INT(m_Stones.flag[4], STONE_REMOVED);
    Arena_Free(&f.arena);
}

TEST(stones_cap_north_damage_and_timeout_only_at_a_pattern_boundary)
{
    FIXTURE f;
    M_Init(&f);
    f.session.stage = 2;
    M_BeginCore(p_BossState, (uint8_t)f.session.rank);
    m_Stones.phase = STONES_NORTH_FIRST;
    m_Stones.pattern = 1;
    m_Stones.flag[4] = STONE_ACTIVE;
    m_Stones.damage[4] = 700;
    CHECK_EQ_INT(
        M_UpdateCore(p_BossState, &f.patterns, (uint8_t)f.session.rank), 1);
    CHECK_EQ_INT(m_Stones.damage[4], 500);
    m_Stones.phase = STONES_NORTH_FINAL;
    m_Stones.pattern = 1;
    m_Stones.frames = 2000;
    m_Stones.sweep_angle = 128;
    p_BossState->phase_frame = 2;
    CHECK_EQ_INT(
        M_UpdateCore(p_BossState, &f.patterns, (uint8_t)f.session.rank), 1);
    CHECK_EQ_INT(m_Stones.flag[4], STONE_ACTIVE);
    CHECK_EQ_INT(
        M_UpdateCore(p_BossState, &f.patterns, (uint8_t)f.session.rank), 1);
    CHECK_EQ_INT(m_Stones.pattern, 2);
    CHECK_EQ_INT(m_Stones.flag[4], STONE_KILL);
    CHECK_EQ_INT(p_Score->delta, 0);
    Arena_Free(&f.arena);
}
