#include "th02/high_score_entry/render.h"

#include "th02/gaiji.h"
#include "th02/high_score_entry/logic.h"
#include "th02/hud.h"

#include "core/base/assert.h"

#include <string.h>

static const uint8_t m_Title[] = { 0xb1, 0xb2, 0xcf, 0xbc, 0xac,
                                   0xb8, 0xbb, 0xae, 0 };
static const uint8_t m_NameHeader[] = { 0xb6, 0xaa, 0xb7, 0xae, 0 };
static const uint8_t m_Point[] = { 0xb9, 0xb8, 0xb2, 0xb6, 0xbd, 0 };
static const uint8_t m_Stage[] = { 0xbc, 0xbd, 0 };

static void M_NameString(
    const HIGH_SCORE_SECTION *const section, const int8_t place,
    const uint16_t attribute)
{
    const uint8_t *const name = section->records[place].name_glyphs;
    ASSERT(memchr(name, 0, HIGH_SCORES_NAME_GLYPHS) != nullptr);
    Tram_PutGaijiString(10, 6 + place, name, attribute);
}

void HighScoreEntryRender_Alphabet(
    const uint8_t col, const uint8_t row, const uint16_t attribute)
{
    Tram_PutGaiji(
        10 + col * 2, 18 + row, HighScoreEntry_Alphabet(col, row), attribute);
}

void HighScoreEntryRender_Table(
    const HIGH_SCORE_SECTION *const section, const bool ending,
    const int8_t place)
{
    ASSERT(place >= -1 && place < HIGH_SCORES_PLACES);
    Tram_PutGaijiString(20, 2, m_Title, TRAM_GREEN);
    Tram_PutGaijiString(12, 4, m_NameHeader, TRAM_GREEN);
    Tram_PutGaijiString(27 + ending, 4, m_Point, TRAM_GREEN);
    Tram_PutGaijiString(41 + ending, 4, m_Stage, TRAM_GREEN);
    if (place != -1) {
        for (uint8_t row = 0; row < 3; row++) {
            for (uint8_t col = 0; col < 17; col++) {
                HighScoreEntryRender_Alphabet(col, row, TRAM_WHITE);
            }
        }
        HighScoreEntryRender_Alphabet(0, 0, 0x85);
    }
    for (int8_t i = 0; i < HIGH_SCORES_PLACES; i++) {
        const uint16_t attribute = i == place ? TRAM_GREEN : TRAM_WHITE;
        const HIGH_SCORE_RECORD *const record = &section->records[i];
        M_NameString(section, i, attribute);
        Hud_Number(26, 6 + i, 8, record->score, attribute, false);
        Tram_PutGaiji(
            44, 6 + i,
            record->stage == 127 ? GAIJI_ALL
                                 : (uint8_t)(GAIJI_DIGIT_0 + record->stage),
            attribute);
    }
    for (int8_t i = 0; i < HIGH_SCORES_PLACES; i++) {
        const uint16_t attribute = i == place ? TRAM_GREEN : TRAM_WHITE;
        if (i < HIGH_SCORES_PLACES - 1) {
            Tram_PutGaiji(6, 6 + i, GAIJI_DIGIT_0 + 1 + i, attribute);
        } else {
            const uint16_t left = 5 - ending;
            Tram_PutGaiji(left, 6 + i, GAIJI_DIGIT_0 + 1, attribute);
            Tram_PutGaiji(left + 2, 6 + i, GAIJI_DIGIT_0, attribute);
        }
    }
}

void HighScoreEntryRender_Name(
    const HIGH_SCORE_SECTION *const section, const int8_t place,
    const uint8_t position)
{
    ASSERT(place >= 0 && place < HIGH_SCORES_PLACES && position < 6);
    M_NameString(section, place, TRAM_GREEN);
    Tram_PutGaiji(
        10 + position * 2, 6 + place,
        section->records[place].name_glyphs[position], 0x85);
}

const HIGH_SCORE_ENTRY_RENDER g_HighScoreEntryRender = {
    .table = HighScoreEntryRender_Table,
    .alphabet = HighScoreEntryRender_Alphabet,
    .name = HighScoreEntryRender_Name,
};
