#include "th02/score_entry/logic.h"

static void M_Table(const SCORE_SECTION *section, bool ending, int8_t place)
{
}
static void M_AlphabetDraw(uint8_t col, uint8_t row, uint16_t attribute)
{
}
static void M_NameDraw(
    const SCORE_SECTION *section, int8_t place, uint8_t position)
{
}

#define ScoreEntryRender_Table M_Table
#define ScoreEntryRender_Alphabet M_AlphabetDraw
#define ScoreEntryRender_Name M_NameDraw
#include "../src/th02/score_entry/logic.c"
#undef ScoreEntryRender_Table
#undef ScoreEntryRender_Alphabet
#undef ScoreEntryRender_Name
