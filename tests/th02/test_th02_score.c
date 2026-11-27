#include "tests/test.h"
#include "tests/th02/th02_score_fixture.h"
#include "tests/th02/th02_sound.h"
#include "tests/th02/th02_tram_fixture.h"

#include <string.h>

TEST(score_transfer_keeps_step_until_empty_and_resets_on_the_following_update)
{
    memset(&m_Score, 0xa5, sizeof(m_Score));
    Score_Init();
    CHECK_EQ_INT(m_Score.value, 0);
    CHECK_EQ_INT(m_Score.delta, 0);
    CHECK_EQ_INT(m_Score.highest, 0);
    CHECK_EQ_INT(m_Score.reset_unknown, 0);
    CHECK_EQ_INT(m_Score.transferred, 0);
    CHECK_EQ_INT(m_Score.extends, 0);
    CHECK_EQ_INT(m_Score.lives, 0);
    CHECK_EQ_INT(m_Score.bombs, 0);
    CHECK_EQ_INT(m_Score.highest_continues, 0);
    m_Score.delta = 32000;
    Score_Update(false);
    CHECK_EQ_INT(m_Score.value, 1000);
    CHECK_EQ_INT(m_Score.transferred, 1000);
    m_Score.delta = 500;
    Score_Update(false);
    CHECK_EQ_INT(m_Score.value, 1500);
    CHECK_EQ_INT(m_Score.delta, 0);
    CHECK_EQ_INT(m_Score.transferred, 500);
    Score_Update(false);
    CHECK_EQ_INT(m_Score.transferred, 0);
    m_Score.delta = 4000;
    Score_Update(true);
    CHECK_EQ_INT(m_Score.transferred, 3111);
    CHECK_EQ_INT(m_Score.delta, 889);
}

TEST(score_update_draws_real_counters_and_awards_only_one_extend)
{
    Score_Init();
    m_Score.value = 799999;
    m_Score.delta = 1;
    m_Score.lives = 5;
    m_Score.bombs = 4;
    memset(&m_Tram, 0xa5, sizeof(m_Tram));
    TEST_SOUND sound;
    TestSound_Init(&sound);
    Score_Update(false);
    CHECK_EQ_INT(m_Score.value, 800000);
    CHECK_EQ_INT(m_Score.highest, 800000);
    CHECK_EQ_INT(m_Score.bombs, 5);
    CHECK_EQ_INT(m_Tram.codes[6 * TRAM_COLUMNS + 58], 0x2857);
    CHECK_EQ_INT(m_Tram.codes[4 * TRAM_COLUMNS + 58], 0x2857);
    CHECK_EQ_INT(m_Tram.codes[15 * TRAM_COLUMNS + 70], 0x0356);
    CHECK_EQ_INT(m_Tram.codes[17 * TRAM_COLUMNS + 62], 0xa5a5);
    CHECK_EQ_INT(m_Score.extends, 1);
    TestSound_Check(&sound, -1);
    SoundEffects_Update();
    TestSound_Check(&sound, 8);
    m_Score.delta = 1;
    m_Tram.codes[15 * TRAM_COLUMNS + 70] = 0xa5a5;
    Score_Update(false);
    CHECK_EQ_INT(m_Score.value, 800001);
    CHECK_EQ_INT(m_Tram.codes[6 * TRAM_COLUMNS + 68], 0x2157);
    CHECK_EQ_INT(m_Tram.codes[15 * TRAM_COLUMNS + 70], 0xa5a5);
    CHECK_EQ_INT(m_Score.extends, 2);
    SoundEffects_Update();
    TestSound_Check(&sound, 8);
    TestSound_Free(&sound);
}

TEST(score_bonus_keeps_delta_and_gives_no_capped_bomb_or_effect)
{
    Score_Init();
    m_Score.value = 99999;
    m_Score.delta = 1;
    m_Score.transferred = 27;
    m_Score.lives = 5;
    m_Score.bombs = 2;
    memset(&m_Tram, 0xa5, sizeof(m_Tram));
    TEST_SOUND sound;
    TestSound_Init(&sound);
    Score_GrantBonus();
    CHECK_EQ_INT(m_Score.delta, 1);
    CHECK_EQ_INT(m_Score.transferred, 27);
    CHECK_EQ_INT(m_Score.extends, 1);
    CHECK_EQ_INT(m_Score.bombs, 2);
    CHECK_EQ_INT(m_Tram.codes[6 * TRAM_COLUMNS + 58], 0x2157);
    CHECK_EQ_INT(m_Tram.codes[4 * TRAM_COLUMNS + 58], 0x2157);
    CHECK_EQ_INT(m_Tram.codes[15 * TRAM_COLUMNS + 62], 0xa5a5);
    CHECK_EQ_INT(m_Tram.codes[17 * TRAM_COLUMNS + 62], 0xa5a5);
    SoundEffects_Update();
    TestSound_Check(&sound, -1);
    Score_GrantBonus();
    CHECK_EQ_INT(m_Score.value, 100001);
    SoundEffects_Update();
    TestSound_Check(&sound, -1);
    TestSound_Free(&sound);
}

TEST(score_partial_initialization_and_wrapped_arithmetic_keep_source_fields)
{
    Score_Init();
    m_Score.value = 200000;
    m_Score.delta = 23;
    m_Score.transferred = 99;
    Score_InitExtends();
    CHECK_EQ_INT(m_Score.extends, 2);
    CHECK_EQ_INT(m_Score.delta, 0);
    CHECK_EQ_INT(m_Score.transferred, 99);
    Score_InitHighest(-19);
    CHECK_EQ_INT(m_Score.highest, 200000);
    CHECK_EQ_INT(m_Score.highest_continues, 247);
    m_Score.value = INT32_MAX;
    m_Score.delta = 1;
    Score_Commit();
    CHECK_EQ_INT(m_Score.value, INT32_MIN);
    CHECK_EQ_INT(m_Score.transferred, 0);
    m_Score.delta = -1;
    Score_Update(false);
    CHECK_EQ_INT(m_Score.transferred, UINT16_MAX);
    CHECK_EQ_INT(m_Score.delta, 0);
    Score_Reset();
    CHECK_EQ_INT(m_Score.highest, 200000);
    CHECK_EQ_INT(m_Score.highest_continues, 247);
    CHECK_EQ_INT(m_Score.extends, 2);
    CHECK_EQ_INT(m_Score.reset_unknown, 40000);
}

TEST(score_updates_preserve_the_live_reads_after_the_last_listed_threshold)
{
    Score_Init();
    m_Score.value = 99999998;
    m_Score.delta = 1;
    m_Score.reset_unknown = 40000;
    m_Score.extends = 5;
    m_Score.lives = 3;
    m_Score.bombs = 3;
    Score_Update(false);
    CHECK_EQ_INT(m_Score.extends, 6);
    CHECK_EQ_INT(m_Score.lives, 4);
    m_Score.delta = 1;
    Score_Update(false);
    CHECK_EQ_INT(m_Score.extends, 7);
    CHECK_EQ_INT(m_Score.lives, 5);
    m_Score.delta = 1;
    Score_Update(false);
    CHECK_EQ_INT(m_Score.extends, 7);
    CHECK_EQ_INT(m_Score.value, 100000001);
    CHECK_EQ_INT(m_Score.bombs, 3);

    Score_Init();
    m_Score.value = 65540;
    m_Score.delta = 1;
    m_Score.reset_unknown = 1;
    m_Score.extends = 6;
    Score_Update(false);
    CHECK_EQ_INT(m_Score.extends, 6);
    m_Score.delta = 1;
    Score_Update(false);
    CHECK_EQ_INT(m_Score.extends, 7);
}

TEST(score_initialization_and_bonus_use_the_adjacent_words)
{
    Score_Init();
    m_Score.value = 100000000;
    m_Score.delta = 77;
    m_Score.reset_unknown = 40000;
    m_Score.transferred = 19;
    Score_InitExtends();
    CHECK_EQ_INT(m_Score.extends, 7);
    CHECK_EQ_INT(m_Score.delta, 0);
    CHECK_EQ_INT(m_Score.transferred, 19);
    m_Score.reset_unknown = 32767;
    Score_InitExtends();
    CHECK_EQ_INT(m_Score.extends, 6);

    Score_Init();
    m_Score.value = 1111495219;
    m_Score.delta = 1;
    m_Score.reset_unknown = 0x12345678;
    m_Score.extends = 7;
    Score_GrantBonus();
    CHECK_EQ_INT(m_Score.extends, 8);
    CHECK_EQ_INT(m_Score.value, 1111495220);
    CHECK_EQ_INT(m_Score.lives, 1);
    CHECK_EQ_INT(m_Score.delta, 1);
}

static uint64_t M_HashValue(uint64_t hash, const uint32_t value)
{
    for (size_t byte = 0; byte < 4; byte++) {
        hash ^= (value >> (byte * 8)) & 255;
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

TEST(real_hud_and_sound_preserve_transfer_bonus_and_threshold_boundaries)
{
    static const SCORE cases[] = {
        { .delta = 32000 },
        { .value = 799999, .delta = 1, .lives = 5, .bombs = 4 },
        { .value = 99999, .delta = 1, .lives = 4 },
        { .value = 99999, .delta = 1, .lives = 5, .bombs = 5 },
        { .value = 99999, .delta = 1, .lives = 5, .bombs = 2 },
        { .value = INT32_MAX, .delta = 1 },
        { .value = 200000, .delta = -1 },
        { .value = 99999998,
          .delta = 1,
          .extends = 5,
          .reset_unknown = 40000,
          .lives = 3,
          .bombs = 3 },
        { .value = 65540, .delta = 1, .reset_unknown = 1, .extends = 6 },
        { .value = 1111495219,
          .delta = 1,
          .reset_unknown = 0x12345678,
          .extends = 7 },
        { .value = 17, .transferred = 123 },
    };
    uint64_t hash = UINT64_C(14695981039346656037);
    for (size_t mode = 0; mode < 3; mode++) {
        for (size_t i = 0; i < ARRAY_SIZE(cases); i++) {
            Score_Init();
            m_Score = cases[i];
            memset(&m_Tram, 0xa5, sizeof(m_Tram));
            TEST_SOUND sound;
            TestSound_Init(&sound);
            for (size_t step = 0; step < 4; step++) {
                m_Score.delta = cases[i].delta;
                if (mode == 2) {
                    Score_GrantBonus();
                } else {
                    Score_Update(mode == 1);
                }
                const int32_t values[] = {
                    m_Score.value,
                    m_Score.delta,
                    m_Score.highest,
                    m_Score.reset_unknown,
                    m_Score.transferred,
                    m_Score.extends,
                    m_Score.lives,
                    m_Score.bombs,
                    m_Score.highest_continues,
                };
                for (size_t j = 0; j < ARRAY_SIZE(values); j++) {
                    hash = M_HashValue(hash, (uint32_t)values[j]);
                }
                for (size_t cell = 0; cell < TRAM_PLANE_CELLS; cell++) {
                    hash = M_HashValue(hash, m_Tram.codes[cell]);
                    hash = M_HashValue(hash, m_Tram.attributes[cell]);
                }
                SoundEffects_Update();
                hash = TestSound_Hash(&sound, hash);
                if (m_Score.extends == 8) {
                    break;
                }
            }
            TestSound_Free(&sound);
        }
    }
    CHECK(hash == UINT64_C(8526527618426675493));
}

TEST(points_and_bonus_add_with_wrap_and_set_and_clear_leave_the_other_field)
{
    Score_Init();
    m_Score.delta = INT32_MAX;
    Score_AddPoints(1);
    CHECK_EQ_INT(m_Score.delta, INT32_MIN);
    m_Score.value = INT32_MAX;
    Score_AddBonus(1);
    CHECK_EQ_INT(m_Score.value, INT32_MIN);
    m_Score.delta = 7;
    Score_Set(500);
    CHECK_EQ_INT(m_Score.value, 500);
    CHECK_EQ_INT(m_Score.delta, 7);
    Score_ClearDelta();
    CHECK_EQ_INT(m_Score.value, 500);
    CHECK_EQ_INT(m_Score.delta, 0);
}

TEST(damage_wraps_in_the_low_word_without_carry)
{
    Score_Init();
    m_Score.delta = 0x1234fffe;
    Score_AddDamage(14);
    CHECK_EQ_INT(m_Score.delta, 0x1234000c);
    m_Score.delta = INT32_MAX;
    Score_AddDamage(1);
    CHECK_EQ_INT(m_Score.delta, 0x7fff0000);
    Score_AddDamage(0);
    CHECK_EQ_INT(m_Score.delta, 0x7fff0000);
}
