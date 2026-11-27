#include "tests/test.h"
#include "tests/th02/th02_clip_fixture.h"
#include "tests/th02/th02_palette_fixture.h"
#include "tests/th02/th02_tram_fixture.h"
#include "th02/high_score_view/render.h"

#include "core/base/memory.h"

#include <string.h>

typedef struct {
    HIGH_SCORES scores;
    TH02_SETTINGS settings;
    HIGH_SCORE_VIEW_ASSETS assets;
} FIXTURE;

static void M_Init(FIXTURE *const fixture)
{
    memset(p_Palette, 0, sizeof((*p_Palette)));
    Tram_Init();
    memset(fixture, 0, sizeof(*fixture));
    Video_Init(&g_HeapAllocator);
    memset(g_Video.pages[0], 7, VIDEO_PAGE_SIZE);
    memset(g_Video.pages[1], 7, VIDEO_PAGE_SIZE);
    memset(p_Tram, 0x5a, sizeof(*p_Tram));
    Clip_Reset();
    Grcg_Init();
    Palette_Init();
    p_Palette->tone = 50;
    HighScores_Defaults(&fixture->scores);
    for (int i = 0; i < 16; i++) {
        fixture->assets.palette[i] = (GFX_COLOR) { i * 17, 0, 0, 255 };
    }
}

static void M_Free(FIXTURE *const fixture)
{
    Mem_Free(g_Video.pages[0]);
}

TEST(begin_preserves_hidden_memory_and_shows_the_inclusive_purple_rectangle)
{
    FIXTURE fixture;
    M_Init(&fixture);
    HighScoreViewRender_Begin(
        &fixture.scores, &fixture.settings, &fixture.assets);
    CHECK_EQ_INT(g_Video.drawing_page, 0);
    CHECK_EQ_INT(g_Video.visible_page, 1);
    CHECK_EQ_INT(p_Palette->tone, 50);
    CHECK_EQ_INT(g_Clip->left, 128);
    CHECK_EQ_INT(g_Clip->right, 512);
    CHECK_EQ_INT(g_Clip->top, 96);
    CHECK_EQ_INT(g_Clip->bottom, 304);
    for (size_t page = 0; page < 2; page++) {
        for (size_t index = 0; index < VIDEO_PAGE_SIZE; index++) {
            const size_t x = index % VIDEO_WIDTH;
            const size_t y = index / VIDEO_WIDTH;
            const uint8_t expected = index >= VIDEO_VISIBLE_SIZE           ? 7
                : page == 1 && x >= 128 && x <= 512 && y >= 96 && y <= 304 ? 10
                                                                           : 0;
            CHECK_EQ_INT(g_Video.pages[page][index], expected);
        }
    }
    CHECK_EQ_INT(p_Tram->codes[2000], 0x5a5a);
    CHECK_EQ_INT(p_Tram->attributes[2000], 0x5a5a);
    CHECK_EQ_INT(g_Grcg->mode, 0);
    HighScoreViewRender_Flip(1, 0);
    CHECK_EQ_INT(g_Video.drawing_page, 1);
    CHECK_EQ_INT(g_Video.visible_page, 0);
    HighScoreViewRender_End();
    CHECK_EQ_INT(g_Clip->left, 0);
    CHECK_EQ_INT(g_Clip->right, 639);
    CHECK_EQ_INT(g_Clip->bottom, 399);
    M_Free(&fixture);
}

TEST(table_uses_live_rank_and_preserves_blank_digits_and_raw_dates)
{
    FIXTURE fixture;
    M_Init(&fixture);
    fixture.settings.difficulty = 2;
    HIGH_SCORE_SECTION *const section = &fixture.scores.sections[2];
    section->records[0].score = 0;
    section->records[1].score = -1;
    section->records[2].stage = 127;
    section->records[0].year = 2026;
    section->records[0].month = 255;
    section->records[0].day = 31;
    const HIGH_SCORES before = fixture.scores;
    HighScoreViewRender_Begin(
        &fixture.scores, &fixture.settings, &fixture.assets);
    for (size_t x = 26; x < 42; x++) {
        CHECK_EQ_INT(p_Tram->codes[7 * 80 + x], 0x5a5a);
    }
    CHECK_EQ_INT(p_Tram->codes[8 * 80 + 40], 0x1f57);
    CHECK_EQ_INT(p_Tram->codes[9 * 80 + 44], 0x7057);
    CHECK_EQ_INT(p_Tram->codes[7 * 80 + 56], '2');
    CHECK_EQ_INT(p_Tram->codes[7 * 80 + 59], '6');
    CHECK_EQ_INT(p_Tram->codes[7 * 80 + 60], '/');
    CHECK_EQ_INT(p_Tram->codes[7 * 80 + 61], '0');
    CHECK_EQ_INT(p_Tram->codes[7 * 80 + 62], '/');
    CHECK_EQ_INT(p_Tram->codes[7 * 80 + 64], '3');
    CHECK_EQ_INT(p_Tram->codes[7 * 80 + 65], '1');
    CHECK_EQ_INT(p_Tram->codes[16 * 80 + 8], 0x2157);
    CHECK_EQ_INT(p_Tram->codes[16 * 80 + 10], 0x2057);
    CHECK(memcmp(&before, &fixture.scores, sizeof(before)) == 0);
    M_Free(&fixture);
}
