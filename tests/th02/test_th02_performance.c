#include "tests/test.h"
#include "th02/performance_private.h"

#include <string.h>

TEST(performance_initialization_and_timed_raises_keep_source_limits)
{
    memset(p_Performance, 0x5a, sizeof(*p_Performance));
    Performance_Init(0);
    CHECK_EQ_INT(p_Performance->maximum, 4);
    CHECK_EQ_INT(p_Performance->value, 0);
    CHECK_EQ_INT(p_Performance->item_skill, 0);
    CHECK_EQ_INT(p_Performance->collected, 0);
    CHECK_EQ_INT(p_Performance->dropped, 0);
    CHECK_EQ_INT(p_Performance->total_misses, 0);
    CHECK_EQ_INT(p_Performance->total_bombs, 0);
    p_Performance->value = 3;
    p_Performance->collected = 31;
    Performance_Advance(1499);
    CHECK_EQ_INT(p_Performance->value, 3);
    Performance_Advance(1500);
    CHECK_EQ_INT(p_Performance->value, 4);
    Performance_Advance(3300);
    CHECK_EQ_INT(p_Performance->value, 4);
    CHECK_EQ_INT(p_Performance->collected, 31);
    Performance_Init(-1);
    CHECK_EQ_INT(p_Performance->maximum, 16);
    p_Performance->value = -6;
    Performance_Advance(1800001500u);
    CHECK_EQ_INT(p_Performance->value, -5);
}

TEST(miss_penalty_keeps_word_wrap_before_its_lower_clamp)
{
    const int16_t values[] = { 3, 2, 0, -5, -6, INT16_MIN };
    const int16_t expected[] = { 0, 0, -2, -6, -6, 32766 };
    for (size_t i = 0; i < sizeof(values) / sizeof(*values); i++) {
        memset(p_Performance, 0, sizeof(*p_Performance));
        p_Performance->value = values[i];
        p_Performance->item_skill = 23;
        p_Performance->total_misses = 255;
        Performance_MissPenalty();
        CHECK_EQ_INT(p_Performance->value, expected[i]);
        CHECK_EQ_INT(p_Performance->item_skill, 23);
        CHECK_EQ_INT(p_Performance->total_misses, 255);
    }
}

TEST(collection_fraction_wraps_and_commit_consumes_only_one_group)
{
    memset(p_Performance, 0, sizeof(*p_Performance));
    p_Performance->item_skill = INT16_MAX;
    p_Performance->collected = 255;
    Performance_AddCollection(1);
    CHECK_EQ_INT(p_Performance->collected, 0);
    Performance_CommitCollection();
    CHECK_EQ_INT(p_Performance->item_skill, INT16_MAX);
    Performance_AddCollection(32);
    CHECK_EQ_INT(p_Performance->item_skill, INT16_MAX);
    Performance_CommitCollection();
    CHECK_EQ_INT(p_Performance->item_skill, INT16_MIN);
    CHECK_EQ_INT(p_Performance->collected, 0);
    p_Performance->collected = 255;
    Performance_CommitCollection();
    CHECK_EQ_INT(p_Performance->collected, 223);
    CHECK_EQ_INT(p_Performance->item_skill, INT16_MIN + 1);
}

TEST(drop_cycle_decrements_skill_on_sixteenth_drop_and_byte_wrap)
{
    memset(p_Performance, 0, sizeof(*p_Performance));
    p_Performance->item_skill = INT16_MIN;
    p_Performance->dropped = 14;
    Performance_Drop();
    CHECK_EQ_INT(p_Performance->item_skill, INT16_MIN);
    Performance_Drop();
    CHECK_EQ_INT(p_Performance->dropped, 16);
    CHECK_EQ_INT(p_Performance->item_skill, INT16_MAX);
    p_Performance->dropped = 255;
    Performance_Drop();
    CHECK_EQ_INT(p_Performance->dropped, 0);
    CHECK_EQ_INT(p_Performance->item_skill, INT16_MAX - 1);
}

TEST(ending_skill_clamps_item_skill_in_place_and_keeps_continue_word)
{
    memset(p_Performance, 0, sizeof(*p_Performance));
    p_Performance->value = -3;
    p_Performance->item_skill = 40;
    p_Performance->total_misses = 2;
    p_Performance->total_bombs = 4;
    p_Performance->collected = 31;
    p_Performance->dropped = 14;
    CHECK_EQ_INT(Performance_Skill(2, 1), 112);
    CHECK_EQ_INT(p_Performance->item_skill, 25);
    CHECK_EQ_INT(Performance_Skill(0, UINT16_MAX), 78);
    CHECK_EQ_INT(p_Performance->value, -3);
    CHECK_EQ_INT(p_Performance->total_misses, 2);
    CHECK_EQ_INT(p_Performance->total_bombs, 4);
    CHECK_EQ_INT(p_Performance->collected, 31);
    CHECK_EQ_INT(p_Performance->dropped, 14);
}
