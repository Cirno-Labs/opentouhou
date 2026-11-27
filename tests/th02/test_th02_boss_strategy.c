#include "th02/boss/fight.h"
#include "th02/boss/strategy.h"
#include "th02/session.h"

#include "test.h"

TEST(get_returns_null_for_every_stage_until_its_boss_lands)
{
    CHECK(BossStrategy_Get(0) != nullptr);
    CHECK(BossStrategy_Get(1) != nullptr);
    CHECK(BossStrategy_Get(2) != nullptr);
    for (uint8_t stage = 3; stage < 6; stage++) {
        CHECK(BossStrategy_Get(stage) == nullptr);
    }
    BossStrategy_InitAll();
}

TEST(fight_without_a_landed_boss_finishes_at_once_without_crashing)
{
    BossFight_Init();
    BossFight_BeginStage();
    TH02_SESSION session = { .stage = 3 };
    CHECK(BossFight_Begin(nullptr, nullptr, nullptr, &session, nullptr));
    CHECK_EQ_INT(
        BossFight_Update(nullptr, nullptr, nullptr, &session, nullptr),
        BOSS_FINISHED);
    CHECK(BossFight_End(nullptr, nullptr, nullptr, &session, nullptr));
    BossFight_Invalidate(session.stage, false);
    CHECK(!BossFight_HasMidboss(session.stage));
    CHECK(!BossFight_MidbossInvalidate(session.stage));
    BossFight_MidbossUpdate(nullptr, session.stage, 0);
}
