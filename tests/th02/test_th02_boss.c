#include "tests/test.h"
#include "tests/th02/th02_boss_hp.h"
#include "tests/th02/th02_palette_fixture.h"
#include "tests/th02/th02_player_fixture.h"
#include "tests/th02/th02_random_fixture.h"
#include "tests/th02/th02_score_fixture.h"
#include "th02/boss.h"
#include "th02/player.h"
#include "th02/random.h"
#include "th02/shots.h"
#include "th02/sparks.c"
#include "th02/sparks.h"
#include "th02/stage.h"
#include "th02/stage/private.h"

#include <string.h>

TEST(init_clears_state_and_sets_the_default_angle)
{
    BOSS_STATE boss;
    memset(&boss, 0xaa, sizeof(boss));
    Boss_Init(&boss);
    CHECK_EQ_INT(boss.angle, 0x20);
    CHECK_EQ_INT(boss.damage, 0);
    CHECK_EQ_INT(boss.defeated, 0);
    CHECK_EQ_INT(boss.hit_flash, 0);
    CHECK_EQ_INT(boss.phase_frame, 0);
}

TEST(begin_stage_keeps_the_angle_and_position_but_clears_the_rest)
{
    BOSS_STATE boss;
    Boss_Init(&boss);
    boss.angle = 7;
    boss.position[0] = (ENTITY_POINT) { 11, 22 };
    boss.phase_frame = 3;
    boss.damage = 4;
    boss.defeated = 1;
    boss.hit_flash = 1;
    Boss_BeginStage(&boss);
    CHECK_EQ_INT(boss.angle, 7);
    CHECK_EQ_INT(boss.position[0].x, 11);
    CHECK_EQ_INT(boss.phase_frame, 0);
    CHECK_EQ_INT(boss.damage, 0);
    CHECK_EQ_INT(boss.defeated, 0);
    CHECK_EQ_INT(boss.hit_flash, 0);
}

TEST(reset_damage_clears_the_counter_and_passes_the_threshold_to_the_hook)
{
    TEST_BOSS_HP hp;
    TestBossHp_Begin(&hp);
    int16_t damage = 1234;
    Boss_ResetDamage(&damage, 500);
    CHECK_EQ_INT(damage, 499);
    CHECK_EQ_INT(hp.count, 1);
    CHECK_EQ_INT(hp.thresholds[0], 500);
    TestBossHp_End();
    damage = 1234;
    Boss_ResetDamage(&damage, 500);
    CHECK_EQ_INT(damage, 0);
}

TEST(hit_reads_live_shots_and_adds_scaled_damage_to_the_running_total)
{
    Player_Init();
    Sparks_Init();
    Score_Init();
    p_Stage->scroll.back = 1;
    p_Stage->world.frame = 2;
    m_Shots.weapon.level = 0;
    SHOT *const shot = &m_Shots.slots[0];
    shot->state = ENTITY_ACTIVE;
    shot->sprite = 50;
    shot->age = 0;
    shot->position[0] = (ENTITY_POINT) { 0, 0 };
    shot->position[1] = (ENTITY_POINT) { 100 * 16, 100 * 16 };
    shot->velocity = (ENTITY_POINT) { -9, 17 };
    int16_t total = 32767;
    const int16_t damage = Boss_Hit(&total, 80, 80, 64, 96, 2, 1);
    CHECK(damage > 0);
    CHECK_EQ_INT(total, (int16_t)(32767 + damage * 2));
    CHECK_EQ_INT(shot->age, 2);
    CHECK_EQ_INT(m_Score.delta, damage);
}

TEST(select_patterns_borrows_the_bank_and_imports_the_optional_palette)
{
    PATTERN_BANK bank = {};
    bank.next = 7;
    uint8_t pixel = 0x12;
    bank.entries[200].data = &pixel;
    PATTERN_BANK target = {};
    Boss_SelectPatterns(&target, &bank, nullptr);
    CHECK_EQ_INT(target.next, 7);
    CHECK(target.entries[200].data == &pixel);
    static const GFX_COLOR palette[16] = {
        { .red = 1, .green = 2, .blue = 3 },
    };
    Boss_SelectPatterns(&target, &bank, palette);
    CHECK_EQ_INT(target.next, 7);
}
