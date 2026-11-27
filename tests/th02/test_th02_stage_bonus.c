#include "th02_score_fixture.h"
#include "th02/stage_bonus/logic.h"
#include "th02/hud.h"
#include "th02/tram.h"

#include "test.h"
#include "th02_sound.h"

typedef struct {
    int16_t rows[10];
    int16_t points;
    int32_t sum_x10;
    int32_t total_x10;
    int32_t score_before;
    int32_t delta_before;
    uint8_t digits;
    int order[32];
    size_t count;
} REQUESTS;

static void M_Begin(void *const context, const bool extra)
{
    REQUESTS *const requests = context;
    requests->order[requests->count++] = extra ? 11 : 10;
}

static void M_Row(
    void *const context, const STAGE_BONUS_LABEL label, const int16_t value)
{
    REQUESTS *const requests = context;
    requests->rows[label] = value;
    requests->order[requests->count++] = 20 + label;
}

static void M_Multiply(
    void *const context, const int16_t points, const int32_t sum_x10,
    const int32_t total_x10, const uint8_t digits)
{
    REQUESTS *const requests = context;
    requests->points = points;
    requests->sum_x10 = sum_x10;
    requests->total_x10 = total_x10;
    requests->digits = digits;
    requests->order[requests->count++] = 30;
    requests->score_before = m_Score.value;
    requests->delta_before = m_Score.delta;
}

TEST(normal_bonus_preserves_request_order_and_awards_existing_delta)
{
    Tram_Init();
    TEST_SOUND sound;
    TestSound_Init(&sound);
    Score_Init();
    m_Score.value = 99999;
    m_Score.delta = 7;
    m_Score.lives = 4;
    const STAGE_BONUS_RUN run = {
        .rank = 1, .point_items = 10, .start_bombs = 3, .start_lives = 3
    };
    const STAGE_BONUS_RUN before = run;
    REQUESTS requests = {};
    const STAGE_BONUS_VIEW view = { M_Begin, M_Row, M_Multiply, &requests };
    const STAGE_BONUS_RESULT result = StageBonus_Grant(&run, false, &view);
    CHECK_EQ_INT(result.sum, 1250);
    CHECK_EQ_INT(result.amount, 12500);
    CHECK_EQ_INT(result.keys, GAME_KEY_UP);
    CHECK_EQ_INT(m_Score.delta, 12507);
    CHECK_EQ_INT(m_Score.value, 112506);
    CHECK_EQ_INT(m_Score.extends, 1);
    CHECK_EQ_INT(m_Score.lives, 5);
    CHECK_EQ_INT(requests.sum_x10, 12500);
    CHECK_EQ_INT(requests.total_x10, 125000);
    CHECK_EQ_INT(requests.digits, 6);
    const int expected[] = { 10, 20, 21, 22, 23, 24, 25, 30 };
    CHECK_EQ_INT(requests.count, sizeof(expected) / sizeof(*expected));
    CHECK(memcmp(requests.order, expected, sizeof(expected)) == 0);
    CHECK(memcmp(&run, &before, sizeof(run)) == 0);
    CHECK_EQ_INT(requests.score_before, 99999);
    CHECK_EQ_INT(requests.delta_before, 7);
    CHECK_EQ_INT(g_Tram->codes[6 * TRAM_COLUMNS + 62], 0x2257);
    CHECK_EQ_INT(g_Tram->codes[17 * TRAM_COLUMNS + 70], 0x0256);
    TestSound_Check(&sound, 8);
    TestSound_Free(&sound);
}

TEST(normal_bonus_wraps_word_rows_before_signed_division)
{
    Score_Init();
    const STAGE_BONUS_RUN run = { .rank = -128,
                                  .performance = INT16_MAX,
                                  .point_items = 1,
                                  .bombs_used = 131,
                                  .misses = 66,
                                  .start_bombs = 255,
                                  .start_lives = 255 };
    REQUESTS requests = {};
    const STAGE_BONUS_VIEW view = { .row = M_Row, .context = &requests };
    const STAGE_BONUS_RESULT result = StageBonus_Grant(&run, false, &view);
    CHECK_EQ_INT(requests.rows[BONUS_RANK], 6144);
    CHECK_EQ_INT(requests.rows[BONUS_PERFORMANCE], 3000);
    CHECK_EQ_INT(requests.rows[BONUS_BOMBS], 2536);
    CHECK_EQ_INT(requests.rows[BONUS_MISSES], 2536);
    CHECK_EQ_INT(requests.rows[BONUS_START_BOMBS], -4192);
    CHECK_EQ_INT(requests.rows[BONUS_START_LIVES], 11144);
    CHECK_EQ_INT(result.sum, 2115);
    CHECK_EQ_INT(result.amount, 2115);
}

TEST(extra_bonus_uses_low_word_time_and_discards_fractional_tens)
{
    STAGE_BONUS_RUN run = { .point_items = 3 };
    const uint32_t frames[] = { 0, 19999, 20000, 20001, 65535, 65536 + 19999 };
    const int32_t amounts[] = { 21000, 15000, 15000, 15000, 21000, 15000 };
    for (size_t i = 0; i < sizeof(frames) / sizeof(*frames); i++) {
        Score_Init();
        run.sigma_frames = frames[i];
        REQUESTS requests = {};
        const STAGE_BONUS_VIEW view = { .multiply = M_Multiply,
                                        .context = &requests };
        const STAGE_BONUS_RESULT result = StageBonus_Grant(&run, true, &view);
        CHECK_EQ_INT(result.amount, amounts[i]);
        CHECK_EQ_INT(requests.digits, 7);
        CHECK_EQ_INT(m_Score.value, amounts[i]);
    }
}

TEST(bonus_keeps_pending_delta_and_transfer_for_repeated_awards)
{
    Score_Init();
    m_Score.value = 100;
    m_Score.delta = 10;
    m_Score.transferred = 33;
    const STAGE_BONUS_RUN run = {};
    StageBonus_Grant(&run, true, nullptr);
    CHECK_EQ_INT(m_Score.value, 110);
    CHECK_EQ_INT(m_Score.delta, 10);
    CHECK_EQ_INT(m_Score.transferred, 33);
    StageBonus_Grant(&run, true, nullptr);
    CHECK_EQ_INT(m_Score.value, 120);
}
