#include "th01/boss_strategy.h"

#include "core/base/assert.h"

extern const BOSS_STRATEGY g_BossSingyoku, g_BossYuugenMagan, g_BossMima,
    g_BossKikuri, g_BossElis, g_BossSariel, g_BossKonngara;

const BOSS_STRATEGY *BossStrategy_Get(const BOSS boss)
{
    static const BOSS_STRATEGY *const strategies[] = {
        [BOSS_SINGYOKU] = &g_BossSingyoku,
        [BOSS_YUUGENMAGAN] = &g_BossYuugenMagan,
        [BOSS_MIMA] = &g_BossMima,
        [BOSS_KIKURI] = &g_BossKikuri,
        [BOSS_ELIS] = &g_BossElis,
        [BOSS_SARIEL] = &g_BossSariel,
        [BOSS_KONNGARA] = &g_BossKonngara,
    };
    ASSERT(boss > BOSS_NONE && boss < BOSS_COUNT);
    return strategies[boss];
}
