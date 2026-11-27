#include "th02/boss/strategy.h"

#include "th02/boss/marisa.h"
#include "th02/boss/meira.h"
#include "th02/boss/rika.h"
#include "th02/boss/stones.h"

#include "core/base/assert.h"

// Each boss commit extends this table with its own entry, in stage order
// (Rika, Meira, the stones, Marisa, Mima, Sigma); a stage whose boss hasn't
// landed yet keeps its nullptr.
static const BOSS_STRATEGY *const m_Strategies[6] = {
    &g_BossRika, &g_BossMeira, &g_BossStones, &g_BossMarisa, nullptr, nullptr,
};

const BOSS_STRATEGY *BossStrategy_Get(const uint8_t stage)
{
    ASSERT(stage < ARRAY_SIZE(m_Strategies));
    return m_Strategies[stage];
}

void BossStrategy_InitAll(void)
{
    for (size_t i = 0; i < ARRAY_SIZE(m_Strategies); i++) {
        if (m_Strategies[i] != nullptr) {
            m_Strategies[i]->init();
        }
    }
}
