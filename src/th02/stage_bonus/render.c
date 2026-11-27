#include "th02/stage_bonus/render.h"
#include "th02/hud.h"
#include "th02/palette.h"
#include "th02/tram.h"

#include "core/base/assert.h"

static const uint8_t m_Title[] = { 0xab, 0xb8, 0xb6, 0xbe, 0xbc, 0 };

static void M_Begin(void *const context, const bool extra)
{
    Hud_WipePlayfield();
    Palette_SetTone(62);
    Tram_PutGaijiString(24, 4, m_Title, 0xe1);
}

static void M_Row(
    void *const context, const STAGE_BONUS_LABEL label, const int16_t value)
{
    ASSERT(label >= BONUS_RANK && label <= BONUS_EXTRA_TIME);
    const STAGE_BONUS_ASSETS *const assets = context;
    const uint16_t row =
        label < BONUS_EXTRA_CLEAR ? label : label - BONUS_EXTRA_CLEAR;
    const uint16_t y = 6 + row * 2;
    Tram_PutText(8, y, &assets->labels[label], UINT16_MAX, 0xe1);
    Hud_Number(22, y, 5, value, TRAM_WHITE, true);
}

static void M_Multiply(
    void *const context, const int16_t points, const int32_t sum_x10,
    const int32_t total_x10, const uint8_t digits)
{
    ASSERT(digits == 6 || digits == 7);
    const STAGE_BONUS_ASSETS *const assets = context;
    Tram_PutText(6, 18, &assets->point, UINT16_MAX, 0xe1);
    Hud_Number(10, 18, 3, points, TRAM_WHITE, true);
    Tram_PutText(18, 18, &assets->times, UINT16_MAX, 0xe1);
    Hud_Number(22, 18, 5, sum_x10, TRAM_WHITE, true);
    Tram_PutText(34, 18, &assets->equals, UINT16_MAX, 0xe1);
    Hud_Number(50 - digits * 2, 18, digits, total_x10, TRAM_WHITE, true);
}

STAGE_BONUS_VIEW StageBonusRender_View(const STAGE_BONUS_ASSETS *const assets)
{
    return (STAGE_BONUS_VIEW) { M_Begin, M_Row, M_Multiply, (void *)assets };
}
