#include "th02/hud/panel.h"

#include "th02/gaiji.h"
#include "th02/tram.h"

#include "core/base/assert.h"

// Retail's two typos ("MORNAL", "LUMATIC") are kept exactly.
static const uint8_t m_Ranks[5][8] = {
    { GAIJI_LETTER_A + 4, GAIJI_LETTER_A + 0, GAIJI_LETTER_A + 18,
      GAIJI_LETTER_A + 24, GAIJI_BLANK, GAIJI_BLANK, GAIJI_BLANK, 0 },
    { GAIJI_LETTER_A + 12, GAIJI_LETTER_A + 14, GAIJI_LETTER_A + 17,
      GAIJI_LETTER_A + 13, GAIJI_LETTER_A + 0, GAIJI_LETTER_A + 11, GAIJI_BLANK,
      0 },
    { GAIJI_LETTER_A + 7, GAIJI_LETTER_A + 0, GAIJI_LETTER_A + 17,
      GAIJI_LETTER_A + 3, GAIJI_BLANK, GAIJI_BLANK, GAIJI_BLANK, 0 },
    { GAIJI_LETTER_A + 11, GAIJI_LETTER_A + 20, GAIJI_LETTER_A + 12,
      GAIJI_LETTER_A + 0, GAIJI_LETTER_A + 19, GAIJI_LETTER_A + 8,
      GAIJI_LETTER_A + 2, 0 },
    { GAIJI_LETTER_A + 4, GAIJI_LETTER_A + 23, GAIJI_LETTER_A + 19,
      GAIJI_LETTER_A + 17, GAIJI_LETTER_A + 0, GAIJI_BLANK, GAIJI_BLANK, 0 },
};
static const uint8_t m_Score[] = { GAIJI_SCORE_LABEL, GAIJI_SCORE_LABEL + 1,
                                   GAIJI_SCORE_LABEL + 2, 0 };
static const uint8_t m_Highest[] = { GAIJI_HI_LABEL, GAIJI_SCORE_LABEL,
                                     GAIJI_SCORE_LABEL + 1,
                                     GAIJI_SCORE_LABEL + 2, 0 };
static const uint8_t m_Lives[] = { GAIJI_LIVES_LABEL, GAIJI_LIVES_LABEL + 1,
                                   0 };
static const uint8_t m_Bombs[] = { GAIJI_BOMBS_LABEL, GAIJI_BOMBS_LABEL + 1,
                                   0 };
static const uint8_t m_Power[] = { GAIJI_POWER_LABEL, GAIJI_POWER_LABEL + 1,
                                   0 };

uint8_t Hud_Put(
    const SCORE *const score, const uint16_t continues, const uint8_t rank,
    const uint8_t power)
{
    ASSERT(rank < 5);
    Tram_PutGaijiString(61, 5, m_Score, TRAM_YELLOW);
    Hud_Score(6, score->value);
    Hud_Continues(6, (int16_t)continues);
    Tram_PutGaijiString(60, 3, m_Highest, TRAM_YELLOW);
    Hud_Score(4, score->highest);
    Hud_Continues(4, score->highest_continues);
    Tram_PutGaijiString(57, 17, m_Lives, TRAM_YELLOW);
    Hud_Lives(score->lives);
    Tram_PutGaijiString(57, 15, m_Bombs, TRAM_YELLOW);
    Hud_Bombs(score->bombs);
    Tram_PutGaijiString(57, 20, m_Power, TRAM_YELLOW);
    const uint8_t level = Hud_Power(power);
    Tram_PutGaijiString(
        57, 22, m_Ranks[rank],
        rank == 0       ? TRAM_GREEN
            : rank == 1 ? TRAM_CYAN
            : rank == 2 ? TRAM_MAGENTA
                        : TRAM_RED);
    return level;
}
