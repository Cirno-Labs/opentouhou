#include "th02_score_fixture.h"
#include "th02_palette_fixture.h"
#include "th02/stage_bonus/logic.c"
#include "th02/hud.h"

#include "test.h"
#include "th02_sound.h"

typedef struct {
    STAGE_BONUS_ASSETS assets;
    TEST_SOUND sound;
} FIXTURE;

static void M_Init(FIXTURE *const f)
{
    memset(&m_Palette, 0, sizeof(m_Palette));
    memset(&g_VSync, 0, sizeof(g_VSync));
    memset(f, 0, sizeof(*f));
    StageBonus_Init();
    Tram_Init();
    Palette_Init();
    Score_Init();
    m_Score.value = 99990;
    m_Score.delta = 10;
    m_Score.transferred = 17;
    m_Score.lives = 4;
    TestSound_Init(&f->sound);
    SoundEffects_Play(SOUND_EFFECT_ITEM);
}

TEST(bonus_awards_once_then_waits_for_counted_release_and_press)
{
    FIXTURE f;
    M_Init(&f);
    const STAGE_BONUS_RUN run = {
        .rank = 1, .point_items = 2, .start_bombs = 3, .start_lives = 2
    };
    const TH02_INPUT held = { .shot = true };
    const STAGE_BONUS_RESULT result =
        StageBonus_Begin(&f.assets, &run, false, 2, &held);
    CHECK_EQ_INT(result.amount, 2700);
    CHECK_EQ_INT(result.keys, GAME_KEY_UP);
    CHECK_EQ_INT(m_Score.value, 102700);
    CHECK_EQ_INT(m_Score.delta, 2710);
    CHECK_EQ_INT(m_Score.transferred, 17);
    CHECK_EQ_INT(m_Score.lives, 5);
    TestSound_Check(&f.sound, 8);
    CHECK_EQ_INT(m_Palette.tone, 62);
    for (unsigned i = 0; i < 8; i++) {
        CHECK(!StageBonus_Update(&held));
    }
    CHECK_EQ_INT(m_Bonus.wait.key_frames, 0);
    for (unsigned i = 0; i < 8; i++) {
        VSyncIrq_VSync();
        CHECK(!StageBonus_Update(&held));
    }
    CHECK_EQ_INT(m_Bonus.wait.key.phase, KEY_WAIT_RELEASE);
    const TH02_INPUT release = {};
    for (unsigned i = 0; i < 8; i++) {
        VSyncIrq_VSync();
        CHECK(!StageBonus_Update(&release));
    }
    CHECK_EQ_INT(m_Bonus.wait.key.phase, KEY_WAIT_PRESS);
    const TH02_INPUT ignored = { .space = true, .right = true };
    for (unsigned i = 0; i < 8; i++) {
        VSyncIrq_VSync();
        CHECK(!StageBonus_Update(&ignored));
    }
    VSyncIrq_VSync();
    CHECK(!StageBonus_Update(&held));
    VSyncIrq_VSync();
    CHECK(!StageBonus_Update(&held));
    CHECK(!StageBonus_Update(&release));
    CHECK_EQ_INT(m_Score.value, 102700);
    CHECK_EQ_INT(m_Score.delta, 2710);
    CHECK_EQ_INT(m_Score.transferred, 17);
    TestSound_Check(&f.sound, -1);
    CHECK_EQ_INT(m_Palette.tone, 62);
    CHECK_EQ_INT(m_Bonus.waits, 1);
    for (unsigned i = 0; i < 6; i++) {
        VSyncIrq_VSync();
        CHECK(!StageBonus_Update(&held));
    }
    CHECK_EQ_INT(m_Bonus.wait.key.phase, KEY_WAIT_RELEASE);
    for (unsigned i = 0; i < 6; i++) {
        VSyncIrq_VSync();
        CHECK(!StageBonus_Update(&release));
    }
    VSyncIrq_VSync();
    CHECK(!StageBonus_Update(&held));
    VSyncIrq_VSync();
    CHECK(StageBonus_Update(&held));
    CHECK_EQ_INT(m_Score.value, 102700);
    CHECK_EQ_INT(m_Score.delta, 2710);
    CHECK_EQ_INT(m_Score.transferred, 17);
    TestSound_Check(&f.sound, -1);
    const SCORE awarded = *g_Score;
    StageBonus_Init();
    const BONUS empty = {};
    CHECK(memcmp(&m_Bonus, &empty, sizeof(empty)) == 0);
    CHECK(memcmp(g_Score, &awarded, sizeof(awarded)) == 0);
    TestSound_Check(&f.sound, -1);
    TestSound_Free(&f.sound);
}

TEST(extra_bonus_consumes_explicit_boss_duration_without_a_countdown)
{
    FIXTURE f;
    M_Init(&f);
    m_Score.value = m_Score.delta = 0;
    const STAGE_BONUS_RUN run = { .point_items = 1, .sigma_frames = 1234 };
    const TH02_INPUT input = {};
    const STAGE_BONUS_RESULT result =
        StageBonus_Begin(&f.assets, &run, true, 1, &input);
    CHECK_EQ_INT(result.amount, 6876);
    CHECK_EQ_INT(m_Score.value, 6876);
    CHECK_EQ_INT(m_Score.delta, 6876);
    TestSound_Check(&f.sound, -1);
    CHECK(!StageBonus_Update(&input));
    CHECK_EQ_INT(m_Score.value, 6876);
    TH02_INPUT press = { .shot = true };
    for (unsigned frame = 0; frame < 4; frame++) {
        VSyncIrq_VSync();
        CHECK(!StageBonus_Update(&input));
    }
    bool complete = false;
    for (unsigned frame = 0; frame < 4 && !complete; frame++) {
        VSyncIrq_VSync();
        complete = StageBonus_Update(&press);
    }
    CHECK(complete);
    CHECK_EQ_INT(m_Bonus.waits, 0);
    CHECK(StageBonus_Update(nullptr));
    CHECK_EQ_INT(m_Bonus.waits, 0);
    CHECK_EQ_INT(m_Score.value, 6876);
    TestSound_Free(&f.sound);
}
