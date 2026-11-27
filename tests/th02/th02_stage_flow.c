#include "th02_stage_flow.h"

static const TEST_BOSS *m_TestBoss;

static bool M_TestBegin(
    PATTERN_BANK *patterns, const BOSS_ASSETS *assets, const BOSS_MUSIC *music,
    const TH02_SESSION *session, const TH02_INPUT *input);
static void M_TestInvalidate(uint8_t stage, bool reduce_effects);
static BOSS_RESULT M_TestUpdate(
    PATTERN_BANK *patterns, const BOSS_ASSETS *assets, const BOSS_MUSIC *music,
    TH02_SESSION *session, const TH02_INPUT *input);
static bool M_TestEnd(
    PATTERN_BANK *patterns, const BOSS_ASSETS *assets, const BOSS_MUSIC *music,
    const TH02_SESSION *session, const TH02_INPUT *input);

#define BossFight_Begin M_TestBegin
#define BossFight_Invalidate M_TestInvalidate
#define BossFight_Update M_TestUpdate
#define BossFight_End M_TestEnd
#include "../src/th02/stage/flow.c"
#undef BossFight_Begin
#undef BossFight_Invalidate
#undef BossFight_Update
#undef BossFight_End

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
        : BossFight_Begin(patterns, assets, music, session, input);
}

static void M_TestInvalidate(const uint8_t stage, const bool reduce_effects)
{
    if (m_TestBoss != nullptr) {
        int16_t value;
        M_TestCall(TEST_BOSS_DRAW, &value);
        return;
    }
    BossFight_Invalidate(stage, reduce_effects);
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
    return BossFight_Update(patterns, assets, music, session, input);
}

static bool M_TestEnd(
    PATTERN_BANK *const patterns, const BOSS_ASSETS *const assets,
    const BOSS_MUSIC *const music, const TH02_SESSION *const session,
    const TH02_INPUT *const input)
{
    int16_t value;
    return m_TestBoss != nullptr
        ? M_TestCall(TEST_BOSS_END, &value)
        : BossFight_End(patterns, assets, music, session, input);
}

void TestStageFlow_Boss(const TEST_BOSS *const boss)
{
    m_TestBoss = boss;
}
