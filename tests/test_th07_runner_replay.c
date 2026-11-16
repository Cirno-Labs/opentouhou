#include "../replay/th07/runner/replay.h"

#include "test.h"

TEST(stage_1_to_6_map_to_themselves)
{
    for (int32_t stage = 1; stage <= 6; stage++) {
        CHECK_EQ_INT(RunnerReplay_GetStage(stage, 0), stage);
        CHECK_EQ_INT(RunnerReplay_GetStage(stage, 3), stage);
    }
}

TEST(slot_7_is_stage_7_except_on_phantasm)
{
    CHECK_EQ_INT(RunnerReplay_GetStage(7, 0), 7);
    CHECK_EQ_INT(RunnerReplay_GetStage(7, 4), 7);
    CHECK_EQ_INT(RunnerReplay_GetStage(7, 5), 8);
}

TEST(scratch_prefix_names_a_subdirectory)
{
    CHECK(RUNNER_SCRATCH_PREFIX[0] != '\0');
}
