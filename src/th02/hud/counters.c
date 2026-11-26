#include "th02/hud/counters.h"

#include "th02/gaiji.h"
#include "th02/tram.h"

#include "core/base/assert.h"

static const uint8_t m_Levels[] = {
    0, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 6, 7, 7, 7, 8, 8, 8, 8, 9,
};
static const uint8_t m_Colors[] = {
    TRAM_RED,  TRAM_RED,   TRAM_RED,  TRAM_MAGENTA, TRAM_MAGENTA,
    TRAM_BLUE, TRAM_GREEN, TRAM_CYAN, TRAM_YELLOW,
};
static const uint8_t m_Max[] = {
    GAIJI_MAX, GAIJI_MAX + 1, GAIJI_MAX + 2, GAIJI_MAX + 3, GAIJI_MAX + 4, 0,
};

static void M_Tally(const uint16_t y, const int8_t value, const uint8_t glyph)
{
    for (int i = 0; i < value; i++) {
        Tram_PutGaiji((uint16_t)(62 + i * 2), y, glyph, TRAM_WHITE);
    }
    for (int i = value; i < 5; i++) {
        Tram_PutGaiji((uint16_t)(62 + i * 2), y, GAIJI_BLANK, TRAM_WHITE);
    }
}

void Hud_Score(const uint16_t y, int32_t value)
{
    int32_t divisor = 1000000;
    for (uint16_t x = 56; x < 70; x += 2) {
        const int16_t numeral = (int16_t)(value / divisor);
        value -= numeral * divisor;
        divisor /= 10;
        Tram_PutGaiji(x, y, (uint8_t)(GAIJI_DIGIT_0 + numeral), TRAM_WHITE);
    }
}

void Hud_Continues(const uint16_t y, const int16_t value)
{
    Tram_PutGaiji(
        70, y, (uint8_t)(GAIJI_DIGIT_0 + (value >= 10 ? 9 : value)),
        TRAM_WHITE);
}

void Hud_Lives(const int8_t value)
{
    M_Tally(17, value, GAIJI_LIFE);
}

void Hud_Bombs(const int8_t value)
{
    M_Tally(15, value, GAIJI_BOMB);
}

uint8_t Hud_Power(const uint8_t power)
{
    ASSERT(power < 84);
    const uint8_t level = m_Levels[power >> 2];
    if (level == 9) {
        Tram_PutGaijiString(62, 20, m_Max, TRAM_WHITE);
    } else {
        uint8_t bar[] = {
            GAIJI_BLANK, GAIJI_BLANK, GAIJI_BLANK, GAIJI_BLANK, GAIJI_BLANK, 0,
        };
        int remaining = (int)power - 16;
        int i = 0;
        while (remaining > 0) {
            bar[i++] = GAIJI_POWER_BAR + 15;
            remaining -= 16;
        }
        bar[i] = GAIJI_POWER_BAR + (((int)power - 1) & 15);
        Tram_PutGaijiString(62, 20, bar, m_Colors[level]);
    }
    return level;
}
