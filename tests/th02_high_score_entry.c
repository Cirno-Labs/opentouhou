#include "th02/high_score_entry/logic.h"

static void M_Table(
    const HIGH_SCORE_SECTION *section, bool ending, int8_t place)
{
}
static void M_AlphabetDraw(uint8_t col, uint8_t row, uint16_t attribute)
{
}
static void M_NameDraw(
    const HIGH_SCORE_SECTION *section, int8_t place, uint8_t position)
{
}

#define HighScoreEntryRender_Table M_Table
#define HighScoreEntryRender_Alphabet M_AlphabetDraw
#define HighScoreEntryRender_Name M_NameDraw
#include "../src/th02/high_score_entry/logic.c"
#undef HighScoreEntryRender_Table
#undef HighScoreEntryRender_Alphabet
#undef HighScoreEntryRender_Name
