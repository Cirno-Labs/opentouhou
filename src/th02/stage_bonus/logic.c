#include "th02/stage_bonus/logic.h"

#include "th02/score.h"
#include "th02/sound_effects.h"
#include "th02/stage_bonus/render.h"
#include "th02/wait.h"

#include "core/base/assert.h"

#include <string.h>

typedef struct {
    WAIT wait;
    bool complete;
    uint8_t waits;
} BONUS;

static BONUS m_Bonus;

static int16_t M_Word(const int32_t value)
{
    return (int16_t)(uint16_t)value;
}

static int16_t M_Positive(const int32_t value)
{
    const int16_t word = M_Word(value);
    return word < 0 ? 0 : word;
}

static void M_Row(
    const STAGE_BONUS_VIEW *const view, const STAGE_BONUS_LABEL label,
    int16_t *const sum, const int16_t value)
{
    if (view != nullptr && view->row != nullptr) {
        view->row(view->context, label, value);
    }
    *sum = M_Word((int32_t)*sum + value / 10);
}

static void M_Wait(const TH02_INPUT *const input)
{
    m_Bonus.complete = false;
    Wait_Key(&m_Bonus.wait, input);
}

void StageBonus_Init(void)
{
    memset(&m_Bonus, 0, sizeof(m_Bonus));
}

STAGE_BONUS_RESULT StageBonus_Grant(
    const STAGE_BONUS_RUN *const run, const bool extra,
    const STAGE_BONUS_VIEW *const view)
{
    int16_t sum = 0;
    if (view != nullptr && view->begin != nullptr) {
        view->begin(view->context, extra);
    }
    if (extra) {
        M_Row(view, BONUS_EXTRA_CLEAR, &sum, 10000);
        M_Row(
            view, BONUS_EXTRA_MISSES, &sum,
            M_Positive(20000 - run->misses * 4000));
        M_Row(
            view, BONUS_EXTRA_BOMBS, &sum,
            M_Positive(20000 - run->bombs_used * 4000));
        const int16_t time =
            M_Positive((int32_t)(uint16_t)(20000u - run->sigma_frames));
        M_Row(view, BONUS_EXTRA_TIME, &sum, time / 10 * 10);
    } else {
        M_Row(view, BONUS_RANK, &sum, M_Word(run->rank * 2000));
        M_Row(
            view, BONUS_PERFORMANCE, &sum,
            M_Word(((int32_t)run->performance + 16) * 200));
        M_Row(
            view, BONUS_BOMBS, &sum, M_Positive(2500 - run->bombs_used * 500));
        M_Row(view, BONUS_MISSES, &sum, M_Positive(3000 - run->misses * 1000));
        M_Row(
            view, BONUS_START_BOMBS, &sum,
            M_Word((4 - run->start_bombs) * 800));
        M_Row(
            view, BONUS_START_LIVES, &sum,
            M_Word((4 - run->start_lives) * 1000));
        if (sum > 25600) {
            sum = 25600;
        }
    }
    const int32_t sum_x10 = (int32_t)sum * 10;
    const int32_t total_x10 =
        (int32_t)((uint32_t)sum_x10 * (uint32_t)(int32_t)run->point_items);
    if (view != nullptr && view->multiply != nullptr) {
        view->multiply(
            view->context, run->point_items, sum_x10, total_x10, extra ? 7 : 6);
    }
    const int32_t amount = total_x10 / 10;
    Score_AddPoints((uint32_t)amount);
    SoundEffects_Reset();
    Score_GrantBonus();
    SoundEffects_Update();
    return (STAGE_BONUS_RESULT) { sum, amount, GAME_KEY_UP };
}

STAGE_BONUS_RESULT StageBonus_Begin(
    const STAGE_BONUS_ASSETS *const assets, const STAGE_BONUS_RUN *const run,
    const bool extra, const uint8_t waits, const TH02_INPUT *const input)
{
    ASSERT(waits != 0);
    StageBonus_Init();
    m_Bonus.waits = waits;
    const STAGE_BONUS_VIEW view = StageBonusRender_View(assets);
    const STAGE_BONUS_RESULT result = StageBonus_Grant(run, extra, &view);
    M_Wait(input);
    return result;
}

bool StageBonus_Update(const TH02_INPUT *const input)
{
    if (m_Bonus.complete) {
        return true;
    }
    m_Bonus.complete = Wait_Ready(&m_Bonus.wait, input, 0);
    if (m_Bonus.complete && --m_Bonus.waits != 0) {
        M_Wait(input);
    }
    return m_Bonus.complete;
}
