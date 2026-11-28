#include "tests/th02/th02_stage_flow.h"

static const TEST_BOSS *m_TestBoss;

static bool M_TestCall(const TEST_BOSS_CALL call, int16_t *const value)
{
    *value = call == TEST_BOSS_UPDATE ? 1 : 0;
    return m_TestBoss->call == nullptr
        || m_TestBoss->call(m_TestBoss->context, call, value);
}

static bool M_TestBegin(
    PATTERN_BANK *const patterns, const BOSS_ASSETS *const assets,
    const BOSS_MUSIC *const music, const TH02_SESSION *const session,
    const TH02_INPUT *const input)
{
    int16_t value;
    return m_TestBoss != nullptr
        ? M_TestCall(TEST_BOSS_BEGIN, &value)
        : g_StageFlowBoss.begin(patterns, assets, music, session, input);
}

static void M_TestInvalidate(const uint8_t stage, const bool reduce_effects)
{
    if (m_TestBoss != nullptr) {
        int16_t value;
        M_TestCall(TEST_BOSS_DRAW, &value);
        return;
    }
    g_StageFlowBoss.invalidate(stage, reduce_effects);
}

static BOSS_RESULT M_TestUpdate(
    PATTERN_BANK *const patterns, const BOSS_ASSETS *const assets,
    const BOSS_MUSIC *const music, TH02_SESSION *const session,
    const TH02_INPUT *const input)
{
    if (m_TestBoss != nullptr) {
        int16_t value;
        return M_TestCall(TEST_BOSS_UPDATE, &value) ? (BOSS_RESULT)value
                                                    : BOSS_WAIT;
    }
    return g_StageFlowBoss.update(patterns, assets, music, session, input);
}

static bool M_TestEnd(
    PATTERN_BANK *const patterns, const BOSS_ASSETS *const assets,
    const BOSS_MUSIC *const music, const TH02_SESSION *const session,
    const TH02_INPUT *const input)
{
    int16_t value;
    return m_TestBoss != nullptr
        ? M_TestCall(TEST_BOSS_END, &value)
        : g_StageFlowBoss.end(patterns, assets, music, session, input);
}

static const STAGE_FLOW_BOSS m_TestBossHooks = {
    .begin = M_TestBegin,
    .invalidate = M_TestInvalidate,
    .update = M_TestUpdate,
    .end = M_TestEnd,
};

void TestStageFlow_Boss(const TEST_BOSS *const boss)
{
    m_TestBoss = boss;
}

void TestStageFlow_Init(void)
{
    StageFlow_Init(&m_TestBossHooks);
}
