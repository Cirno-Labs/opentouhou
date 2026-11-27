#include "tests/test.h"
#include "tests/th02/th02_palette_fixture.h"
#include "tests/th02/th02_stage_flow.c"
#include "th02/enemies.c"
#include "th02/enemies.h"
#include "th02/stage.h"
#include "th02/stage/private.h"

#include <string.h>

typedef struct {
    TH02_SESSION session;
    TH02_INPUT input;
    TEST_BOSS boss;
    unsigned begins;
    unsigned ends;
    unsigned draws;
    unsigned updates;
    unsigned begin_wait;
    unsigned end_wait;
    int16_t progression;
} FIXTURE;

static bool M_Boss(
    void *const context, const TEST_BOSS_CALL call, int16_t *const value)
{
    FIXTURE *const f = context;
    switch (call) {
    case TEST_BOSS_BEGIN:
        CHECK_EQ_INT(g_Stage->world.progression, 1);
        CHECK(m_Flow.enemy_updates);
        CHECK_EQ_INT(m_Enemies.count, 3);
        f->begins++;
        if (f->begin_wait != 0) {
            f->begin_wait--;
            return false;
        }
        break;

    case TEST_BOSS_END:
        CHECK_EQ_INT(g_Stage->world.progression, 2);
        CHECK(!m_Flow.enemy_updates);
        f->ends++;
        if (f->end_wait != 0) {
            f->end_wait--;
            return false;
        }
        break;

    case TEST_BOSS_DRAW:
        f->draws++;
        break;

    case TEST_BOSS_UPDATE:
        f->updates++;
        *value = f->progression;
        break;
    }
    return true;
}

static void M_Init(FIXTURE *const f, const uint8_t stage, const bool reduced)
{
    memset(&m_Palette, 0, sizeof(m_Palette));
    memset(f, 0, sizeof(*f));
    Stage_Init();
    Stage_Begin(stage, reduced);
    f->session.stage = stage;
    Palette_Init();
    Enemies_Init();
    memset(m_Palette.colors[0], 0x70, 3);
    m_Enemies.count = 3;
    m_Enemies.slots[0].state = ENTITY_ACTIVE;
    m_Enemies.slots[1].state = ENTITY_REMOVE;
    m_Enemies.slots[2].state = ENTITY_FREE;
    p_Stage->world.frame = 159;
    p_Stage->scroll.rows = 123;
    p_Stage->scroll.cycle = 7;
    f->boss = (TEST_BOSS) { M_Boss, f };
    StageFlow_Begin(stage, reduced);
    TestStageFlow_Boss(&f->boss);
}

TEST(stage_flow_selects_rules_and_does_not_start_a_boss_early)
{
    static const int16_t rows[] = { 116, 80, 103, 944, -1, 200 };
    for (uint8_t stage = 0; stage < 6; stage++) {
        for (unsigned reduced = 0; reduced < 2; reduced++) {
            FIXTURE f;
            M_Init(&f, stage, reduced != 0);
            CHECK_EQ_INT(g_Stage->world.midboss_row, rows[stage]);
            CHECK_EQ_INT(g_Stage->scroll.speed, stage == 3 ? 2 : 1);
            CHECK_EQ_INT(
                g_Stage->scroll.interval,
                stage == 3 || stage == 4 ? 1
                    : stage == 5         ? 2
                                         : 4);
            CHECK_EQ_INT(g_Stage->world.frame, 159);
            CHECK_EQ_INT(g_Stage->scroll.rows, 123);
            CHECK_EQ_INT(g_Stage->scroll.cycle, 7);
            CHECK(m_Flow.enemy_updates);
            CHECK_EQ_INT(
                m_Flow.after_tile_effect,
                stage < 3 || (stage == 3 && reduced == 0));
            CHECK_EQ_INT(
                StageFlow_Enter(
                    nullptr, nullptr, nullptr, &f.session, &f.input),
                STAGE_FLOW_RUNNING);
            CHECK_EQ_INT(g_Stage->world.progression, 0);
            p_Stage->scroll.done = 2;
            CHECK_EQ_INT(
                StageFlow_Enter(
                    nullptr, nullptr, nullptr, &f.session, &f.input),
                STAGE_FLOW_RUNNING);
            CHECK_EQ_INT(f.begins, 0);
            CHECK_EQ_INT(
                StageFlow_Update(
                    nullptr, nullptr, nullptr, &f.session, &f.input),
                STAGE_FLOW_RUNNING);
            CHECK_EQ_INT(g_Stage->world.progression, 0);
            p_Stage->world.progression = 2;
            CHECK_EQ_INT(
                StageFlow_End(nullptr, nullptr, nullptr, &f.session, &f.input),
                STAGE_FLOW_RUNNING);
            CHECK_EQ_INT(f.begins, 0);
            CHECK_EQ_INT(f.updates, 0);
            CHECK_EQ_INT(f.ends, 0);
        }
    }
}

TEST(stage_flow_preserves_entry_and_exit_order_across_nested_waits)
{
    for (uint8_t stage = 0; stage < 6; stage++) {
        FIXTURE f;
        M_Init(&f, stage, false);
        f.begin_wait = f.end_wait = 1;
        p_Stage->scroll.done = 1;
        CHECK_EQ_INT(
            StageFlow_Enter(nullptr, nullptr, nullptr, &f.session, &f.input),
            STAGE_FLOW_WAIT);
        CHECK_EQ_INT(m_Enemies.count, 3);
        CHECK_EQ_INT(m_Enemies.slots[0].state, ENTITY_ACTIVE);
        CHECK_EQ_INT(g_Stage->scroll.cycle, 7);
        CHECK_EQ_INT(m_Palette.colors[0][0], stage == 2 ? 0 : 0x70);
        CHECK_EQ_INT(g_Stage->world.frame, 159);
        CHECK_EQ_INT(
            StageFlow_Enter(nullptr, nullptr, nullptr, &f.session, &f.input),
            STAGE_FLOW_BOSS_STARTED);
        CHECK_EQ_INT(m_Enemies.count, 0);
        CHECK_EQ_INT(m_Enemies.slots[0].state, ENTITY_REMOVE);
        CHECK_EQ_INT(m_Enemies.slots[1].state, ENTITY_REMOVE);
        CHECK_EQ_INT(m_Enemies.slots[2].state, ENTITY_FREE);
        CHECK_EQ_INT(g_Stage->scroll.cycle, 255);
        CHECK(!m_Flow.enemy_updates);
        CHECK(!m_Flow.after_tile_effect || stage != 3);
        CHECK_EQ_INT(
            StageFlow_Enter(nullptr, nullptr, nullptr, &f.session, &f.input),
            STAGE_FLOW_RUNNING);
        CHECK_EQ_INT(f.begins, 2);
        StageFlow_Invalidate(f.session.stage, false);
        CHECK_EQ_INT(f.draws, 1);
        f.progression = 1;
        CHECK_EQ_INT(
            StageFlow_Update(nullptr, nullptr, nullptr, &f.session, &f.input),
            STAGE_FLOW_RUNNING);
        CHECK_EQ_INT(g_Stage->world.progression, 1);
        CHECK_EQ_INT(
            StageFlow_End(nullptr, nullptr, nullptr, &f.session, &f.input),
            STAGE_FLOW_RUNNING);
        p_Stage->world.progression = 2;
        memset(m_Palette.colors[0], 0x90, 3);
        CHECK_EQ_INT(
            StageFlow_End(nullptr, nullptr, nullptr, &f.session, &f.input),
            STAGE_FLOW_WAIT);
        CHECK_EQ_INT(m_Palette.colors[0][0], stage == 2 ? 0 : 0x90);
        CHECK_EQ_INT(
            StageFlow_End(nullptr, nullptr, nullptr, &f.session, &f.input),
            STAGE_FLOW_CLEARED);
        CHECK_EQ_INT(
            StageFlow_End(nullptr, nullptr, nullptr, &f.session, &f.input),
            STAGE_FLOW_CLEARED);
        CHECK_EQ_INT(f.ends, 2);
        CHECK_EQ_INT(g_Stage->world.frame, 159);
    }
}

TEST(stage_flow_keeps_a_running_boss_until_an_explicit_clear)
{
    FIXTURE f;
    M_Init(&f, 5, false);
    f.boss.call = nullptr;
    p_Stage->scroll.done = 1;
    CHECK_EQ_INT(
        StageFlow_Enter(nullptr, nullptr, nullptr, &f.session, &f.input),
        STAGE_FLOW_BOSS_STARTED);
    for (unsigned i = 0; i < 20; i++) {
        CHECK_EQ_INT(
            StageFlow_Update(nullptr, nullptr, nullptr, &f.session, &f.input),
            STAGE_FLOW_RUNNING);
        CHECK_EQ_INT(g_Stage->world.progression, 1);
        CHECK_EQ_INT(
            StageFlow_End(nullptr, nullptr, nullptr, &f.session, &f.input),
            STAGE_FLOW_RUNNING);
    }
    p_Stage->world.progression = 2;
    CHECK_EQ_INT(
        StageFlow_End(nullptr, nullptr, nullptr, &f.session, &f.input),
        STAGE_FLOW_CLEARED);
    CHECK_EQ_INT(f.ends, 0);
}
