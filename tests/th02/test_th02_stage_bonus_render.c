#include "tests/test.h"
#include "tests/th02/th02_palette_fixture.h"
#include "tests/th02/th02_score_fixture.h"
#include "tests/th02/th02_sound.h"
#include "tests/th02/th02_tram_fixture.h"
#include "th02/hud.h"
#include "th02/stage_bonus/render.h"

TEST(overlay_wipe_preserves_hud_borders_and_plane_tails)
{
    memset(&m_Tram, 0x5a, sizeof(m_Tram));
    Hud_WipePlayfield();
    for (size_t i = 0; i < TRAM_PLANE_CELLS; i++) {
        const size_t x = i % 80;
        const size_t y = i / 80;
        const bool playfield = x >= 4 && x < 52 && y >= 1 && y < 24;
        CHECK_EQ_INT(m_Tram.codes[i], playfield ? 0x20 : 0x5a5a);
        CHECK_EQ_INT(m_Tram.attributes[i], playfield ? 0xe1 : 0x5a5a);
    }
}

TEST(overlay_numbers_force_zero_and_keep_skipped_cells)
{
    memset(&m_Tram, 0x5a, sizeof(m_Tram));
    const TRAM before = m_Tram;
    Hud_Number(4, 6, 0, 12345, TRAM_WHITE, true);
    CHECK(memcmp(&m_Tram, &before, sizeof(m_Tram)) == 0);
    Hud_Number(4, 6, 5, 0, TRAM_WHITE, true);
    CHECK_EQ_INT(m_Tram.codes[6 * 80 + 4], 0x5a5a);
    CHECK_EQ_INT(m_Tram.attributes[6 * 80 + 10], 0x5a5a);
    CHECK_EQ_INT(m_Tram.codes[6 * 80 + 12], 0x2057);
    Hud_Number(4, 6, 5, -1, TRAM_WHITE, true);
    CHECK_EQ_INT(m_Tram.codes[6 * 80 + 12], 0x1f57);
    Hud_Number(0, 0, 3, 123456, TRAM_WHITE, true);
    CHECK_EQ_INT(m_Tram.codes[0], 0x2457);
    CHECK_EQ_INT(m_Tram.codes[2], 0x2557);
    CHECK_EQ_INT(m_Tram.codes[4], 0x2657);
}

TEST(bonus_rows_and_equation_keep_original_positions)
{
    Tram_Init();
    const TRAM_CHARACTER character = { .code = 'L', .source_end = 1 };
    STAGE_BONUS_ASSETS assets = {};
    assets.labels[BONUS_EXTRA_TIME] = (TRAM_TEXT) {
        .text = "L", .characters = &character, .count = 1, .source_size = 1
    };
    const STAGE_BONUS_VIEW view = StageBonusRender_View(&assets);
    view.row(view.context, BONUS_EXTRA_TIME, 10);
    CHECK_EQ_INT(m_Tram.codes[12 * 80 + 8], 'L');
    CHECK_EQ_INT(m_Tram.codes[12 * 80 + 28], 0x2157);
    CHECK_EQ_INT(m_Tram.codes[12 * 80 + 30], 0x2057);
    view.multiply(view.context, 123, 12345, 123456, 6);
    CHECK_EQ_INT(m_Tram.codes[18 * 80 + 10], 0x2157);
    CHECK_EQ_INT(m_Tram.codes[18 * 80 + 22], 0x2157);
    CHECK_EQ_INT(m_Tram.codes[18 * 80 + 38], 0x2157);
    view.multiply(view.context, 123, 12345, 1234567, 7);
    CHECK_EQ_INT(m_Tram.codes[18 * 80 + 36], 0x2157);
    CHECK_EQ_INT(m_Tram.codes[18 * 80 + 48], 0x2757);
}

TEST(bonus_render_composes_palette_score_hud_and_ordered_sound_requests)
{
    memset(&m_Tram, 0x5a, sizeof(m_Tram));

    Palette_Init();
    const PALETTE_STATE before = m_Palette;
    memset(&g_Video, 0, sizeof(g_Video));
    g_Video.drawing_page = 1;
    g_Video.visible_page = 1;
    g_Video.first_lines = 123;
    g_Video.first_address = 456;
    g_Video.second_address = 789;
    const STAGE_BONUS_ASSETS assets = {};
    TEST_SOUND sound;
    TestSound_Init(&sound);
    SoundEffects_Play(SOUND_EFFECT_ITEM);
    const STAGE_BONUS_VIEW view = StageBonusRender_View(&assets);
    Score_Init();
    m_Score.value = 99999;
    m_Score.lives = 5;
    const STAGE_BONUS_RUN run = {
        .rank = 1, .point_items = 10, .start_bombs = 3, .start_lives = 3
    };
    StageBonus_Grant(&run, false, &view);
    CHECK_EQ_INT(m_Score.value, 112499);
    TestSound_Check(&sound, 8);
    TestSound_Free(&sound);
    CHECK_EQ_INT(m_Palette.tone, 62);
    CHECK(
        memcmp(m_Palette.colors, before.colors, sizeof(m_Palette.colors)) == 0);
    CHECK_EQ_INT(g_Video.palette[15].red, 102);
    CHECK_EQ_INT(g_Video.drawing_page, 1);
    CHECK_EQ_INT(g_Video.visible_page, 1);
    CHECK_EQ_INT(g_Video.first_lines, 123);
    CHECK_EQ_INT(g_Video.first_address, 456);
    CHECK_EQ_INT(g_Video.second_address, 789);
    CHECK_EQ_INT(m_Tram.codes[4 * 80 + 24], 0x2b57);
}
