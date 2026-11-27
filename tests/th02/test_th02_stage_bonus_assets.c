#include "tests/test.h"
#include "tests/test_allocator.h"
#include "th02/stage_bonus/assets.h"

TEST(bonus_cache_decodes_the_built_in_labels_and_frees_cleanly)
{
    TEST_ALLOCATOR owned;
    TestAllocator_Init(&owned);
    STAGE_BONUS_ASSETS assets;
    CHECK(Result_Ignore(StageBonusAssets_Load(&owned.allocator, &assets)));
    for (size_t i = 0; i < 10; i++) {
        CHECK(assets.labels[i].count > 0);
    }
    CHECK(assets.point.count > 0);
    CHECK(assets.times.count > 0);
    CHECK(assets.equals.count > 0);
    StageBonusAssets_Free(&assets);
    StageBonusAssets_Free(&assets);
    CHECK_EQ_INT(owned.live_blocks, 0);
    CHECK_EQ_INT(owned.live_bytes, 0);
    CHECK_EQ_INT(owned.size_errors, 0);
}
