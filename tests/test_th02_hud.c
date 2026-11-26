#include "th02_tram_fixture.h"
#include "th02/gaiji.h"
#include "th02/hud.h"

#include "test.h"

TEST(hud_score_keeps_first_quotient_and_separate_continue_cell)
{
    memset(&m_Tram, 0xa5, sizeof(m_Tram));
    Hud_Score(6, 10000000);
    CHECK_EQ_INT(m_Tram.codes[6 * 80 + 56], 0x2a57);
    for (size_t x = 58; x < 70; x += 2) {
        CHECK_EQ_INT(m_Tram.codes[6 * 80 + x], 0x2057);
    }
    CHECK_EQ_INT(m_Tram.codes[6 * 80 + 70], 0xa5a5);
    Hud_Continues(6, 10);
    CHECK_EQ_INT(m_Tram.codes[6 * 80 + 70], 0x2957);
    Hud_Continues(6, -1);
    CHECK_EQ_INT(m_Tram.codes[6 * 80 + 70], 0x1f57);
    Hud_Score(4, -1);
    CHECK_EQ_INT(m_Tram.codes[4 * 80 + 68], 0x1f57);
}

TEST(hud_tallies_preserve_signed_blanking_and_right_edge_spill)
{
    memset(&m_Tram, 0xa5, sizeof(m_Tram));
    Hud_Lives(-1);
    CHECK_EQ_INT(m_Tram.codes[17 * 80 + 60], 0x4f57);
    CHECK_EQ_INT(m_Tram.codes[17 * 80 + 70], 0x4f57);
    CHECK_EQ_INT(m_Tram.codes[17 * 80 + 58], 0xa5a5);
    Hud_Bombs(10);
    CHECK_EQ_INT(m_Tram.codes[15 * 80 + 62], 0x356);
    CHECK_EQ_INT(m_Tram.codes[16 * 80], 0x356);
    CHECK_EQ_INT(m_Tram.codes[16 * 80 + 1], 0x8356);
    CHECK_EQ_INT(m_Tram.codes[16 * 80 + 2], 0xa5a5);
}

TEST(hud_power_keeps_zero_quirk_and_returns_shot_level)
{
    Tram_Init();
    CHECK_EQ_INT(Hud_Power(0), 0);
    CHECK_EQ_INT(m_Tram.codes[20 * 80 + 62], 0x3f56);
    CHECK_EQ_INT(m_Tram.attributes[20 * 80 + 62], 0x41);
    CHECK_EQ_INT(Hud_Power(1), 0);
    CHECK_EQ_INT(m_Tram.codes[20 * 80 + 62], 0x3056);
    CHECK_EQ_INT(Hud_Power(64), 8);
    CHECK_EQ_INT(m_Tram.attributes[20 * 80 + 62], 0xc1);
    CHECK_EQ_INT(Hud_Power(83), 9);
    CHECK_EQ_INT(m_Tram.codes[20 * 80 + 62], 0x4056);
    CHECK_EQ_INT(m_Tram.codes[20 * 80 + 70], 0x4456);
    CHECK_EQ_INT(m_Tram.attributes[20 * 80 + 62], 0xe1);
}

TEST(numbers_skip_leading_zeros_and_show_the_last_only_when_asked)
{
    Tram_Init();
    Hud_Number(10, 2, 5, 304, TRAM_GREEN, true);
    for (unsigned i = 0; i < 2; i++) {
        CHECK_EQ_INT(m_Tram.codes[2 * TRAM_COLUMNS + 10 + i * 2], 0);
    }
    const uint16_t three = ((GAIJI_DIGIT_0 + 3 - 128) << 8) | 0x57;
    CHECK_EQ_INT(m_Tram.codes[2 * TRAM_COLUMNS + 14], three);
    CHECK_EQ_INT(m_Tram.attributes[2 * TRAM_COLUMNS + 14], TRAM_GREEN);
    Tram_Init();
    Hud_Number(10, 2, 3, 0, TRAM_WHITE, false);
    for (unsigned i = 0; i < 6; i++) {
        CHECK_EQ_INT(m_Tram.codes[2 * TRAM_COLUMNS + 10 + i], 0);
    }
    Hud_Number(10, 2, 3, 0, TRAM_WHITE, true);
    const uint16_t zero = ((GAIJI_DIGIT_0 - 128) << 8) | 0x57;
    CHECK_EQ_INT(m_Tram.codes[2 * TRAM_COLUMNS + 14], zero);
}
