#include "th02/high_score_entry/private.h"

static void M_Table(
    const HIGH_SCORE_SECTION *const section, const bool ending,
    const int8_t place)
{
    (void)section;
    (void)ending;
    (void)place;
}
static void M_AlphabetDraw(
    const uint8_t col, const uint8_t row, const uint16_t attribute)
{
    (void)col;
    (void)row;
    (void)attribute;
}
static void M_NameDraw(
    const HIGH_SCORE_SECTION *const section, const int8_t place,
    const uint8_t position)
{
    (void)section;
    (void)place;
    (void)position;
}

const HIGH_SCORE_ENTRY_RENDER g_TestHighScoreEntryRender = {
    .table = M_Table,
    .alphabet = M_AlphabetDraw,
    .name = M_NameDraw,
};
