#include "th02/boss/fight.h"
#include "th02/boss/private.h"
#include "th02/boss/strategy.h"

static BOSS_STATE m_BossState;
BOSS_STATE *const p_BossState = &m_BossState;

const BOSS_STATE *BossFight_State(void)
{
    return &m_BossState;
}

void BossFight_Init(void)
{
    Boss_Init(&m_BossState);
    BossStrategy_InitAll();
}

void BossFight_BeginStage(void)
{
    Boss_BeginStage(&m_BossState);
}

bool BossFight_Begin(
    PATTERN_BANK *const patterns, const BOSS_ASSETS *const assets,
    const BOSS_MUSIC *const music, const TH02_SESSION *const session,
    const TH02_INPUT *const input)
{
    if (!BossTalk_Update(
            &m_BossState, patterns, assets, music, session, false, input)) {
        return false;
    }
    const BOSS_STRATEGY *const strategy = BossStrategy_Get(session->stage);
    if (strategy != nullptr) {
        strategy->begin(patterns, assets, session);
    }
    return true;
}

void BossFight_Invalidate(const uint8_t stage, const bool reduce_effects)
{
    const BOSS_STRATEGY *const strategy = BossStrategy_Get(stage);
    if (strategy != nullptr) {
        strategy->invalidate(reduce_effects);
    }
}

BOSS_RESULT BossFight_Update(
    PATTERN_BANK *const patterns, const BOSS_ASSETS *const assets,
    const BOSS_MUSIC *const music, TH02_SESSION *const session,
    const TH02_INPUT *const input)
{
    const BOSS_STRATEGY *const strategy = BossStrategy_Get(session->stage);
    if (strategy == nullptr) {
        return BOSS_FINISHED;
    }
    return strategy->update(patterns, assets, music, session, input);
}

bool BossFight_End(
    PATTERN_BANK *const patterns, const BOSS_ASSETS *const assets,
    const BOSS_MUSIC *const music, const TH02_SESSION *const session,
    const TH02_INPUT *const input)
{
    const BOSS_STRATEGY *const strategy = BossStrategy_Get(session->stage);
    return strategy == nullptr || !strategy->post_dialogue
        || BossTalk_Update(
               &m_BossState, patterns, assets, music, session, true, input);
}

bool BossFight_HasMidboss(const uint8_t stage)
{
    const BOSS_STRATEGY *const strategy = BossStrategy_Get(stage);
    return strategy != nullptr && strategy->midboss_update != nullptr;
}

bool BossFight_MidbossInvalidate(const uint8_t stage)
{
    const BOSS_STRATEGY *const strategy = BossStrategy_Get(stage);
    return strategy != nullptr && strategy->midboss_invalidate != nullptr
        && strategy->midboss_invalidate();
}

void BossFight_MidbossUpdate(
    const PATTERN_BANK *const patterns, const uint8_t stage, const uint8_t rank)
{
    const BOSS_STRATEGY *const strategy = BossStrategy_Get(stage);
    if (strategy != nullptr && strategy->midboss_update != nullptr) {
        strategy->midboss_update(patterns, rank);
    }
}
