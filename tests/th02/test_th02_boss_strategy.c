#include "tests/test.h"
#include "th02/boss/strategy.h"

TEST(get_returns_every_landed_bosss_strategy)
{
    for (uint8_t stage = 0; stage < 6; stage++) {
        CHECK(BossStrategy_Get(stage) != nullptr);
    }
    BossStrategy_InitAll();
}
