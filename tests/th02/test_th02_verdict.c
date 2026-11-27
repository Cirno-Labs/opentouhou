#include "tests/test.h"
#include "tests/th02/th02_clip_fixture.h"
#include "tests/th02/th02_palette_fixture.h"
#include "th02/ending/verdict.c"

#include "core/base/arena.h"
#include "core/base/memory.h"

#include <string.h>

typedef struct {
    ARENA arena;

    TH02_SESSION session;
    ENDING_ASSETS assets;
} FIXTURE;

static void M_Init(FIXTURE *const fixture)
{
    memset(&m_Palette, 0, sizeof(m_Palette));
    Grcg_Init();
    memset(&g_VSync, 0, sizeof(g_VSync));
    memset(fixture, 0, sizeof(*fixture));
    Arena_Init(&fixture->arena, "verdict", 2 * 1024 * 1024);
    const ALLOCATOR *const allocator = Arena_Allocator(&fixture->arena);
    Video_Init(allocator);
    Clip_Reset();
    Palette_Init();
    m_Palette.tone = 0;
    VSyncIrq_BeginProgram();
    g_VSync.count1 = 65534;
    g_VSync.count2 = 65530;
    fixture->session.score = 123400;
    fixture->session.skill = 77;
    fixture->session.frame = 1234;
    PI_IMAGE *const image =
        &fixture->assets.images.images[ENDING_IMAGE_VERDICT];
    image->width = 640;
    image->height = 400;
    image->pixels = Allocator_Alloc(allocator, VIDEO_VISIBLE_SIZE);
    for (size_t i = 0; i < VIDEO_VISIBLE_SIZE; i++) {
        image->pixels[i] = (uint8_t)(i % 16);
    }
    for (size_t i = 0; i < 16; i++) {
        image->palette[i] = (GFX_COLOR) { i * 17, 255 - i * 17, i * 7, 255 };
    }
    uint16_t *const rows = Allocator_AllocZ(allocator, 16 * sizeof(*rows));
    rows[0] = 0x8000;
    DRAW_TEXT_GLYPH *const glyph = Allocator_Alloc(allocator, sizeof(*glyph));
    *glyph = (DRAW_TEXT_GLYPH) { rows, true };
    TRAM_CHARACTER *const character =
        Allocator_AllocZ(allocator, sizeof(*character));
    character->source_end = 2;
    for (size_t i = 0; i < ENDING_LABEL_COUNT; i++) {
        fixture->assets.labels.text[i].glyphs = glyph;
        fixture->assets.labels.text[i].count = 1;
        fixture->assets.labels.text[i].cells.characters = character;
    }
    DRAW_TEXT *const titles = Allocator_AllocZ(allocator, 12 * sizeof(*titles));
    for (size_t i = 0; i < 12; i++) {
        titles[i].glyphs = glyph;
        titles[i].count = 1;
        titles[i].cells.characters = character;
    }
    fixture->assets.verdict[ENDING_STORY_GOOD] =
        (ENDING_TEXT_SCRIPT) { titles, 12 };
    for (size_t step = 0; step < ENDING_REVEAL_STEPS; step++) {
        for (size_t y = 0; y < 16; y++) {
            fixture->assets.masks.rows[step][y] = UINT16_MAX;
        }
    }
    Verdict_Begin(&fixture->assets, &fixture->session, ENDING_STORY_GOOD, 0x2f);
}

TEST(verdict_runs_all_rows_with_counter_waits_and_separate_skill_reveals)
{
    const uint16_t row_frames[] = { 169, 399,  529,  659, 789,
                                    919, 1099, 1249, 1479 };
    {
        FIXTURE *const fixture = Mem_Alloc(sizeof(*fixture));
        M_Init(fixture);
        const TH02_SESSION session = fixture->session;
        const TH02_INPUT none = {};
        CHECK_EQ_INT(g_VSync.count1, 65534);
        CHECK(!Verdict_Update(&none));
        CHECK_EQ_INT(m_Verdict.phase, VERDICT_FADE_IN);
        CHECK_EQ_INT(g_VSync.count1, 65534);
        uint16_t accepted = 0;
        uint8_t rows = 0;
        bool done = false;
        for (size_t tick = 0; tick < 3000 && !done; tick++) {
            VSyncIrq_VSync();
            accepted++;
            const TH02_INPUT sample = { .ok = m_Verdict.phase == VERDICT_CONFIRM
                                            && m_Verdict.wait.key.phase
                                                == KEY_WAIT_PRESS };
            const uint8_t before = m_Verdict.reveal.step;
            done = Verdict_Update(&sample);
            if (m_Verdict.phase == VERDICT_REVEAL && m_Verdict.reveal.step == 1
                && before != 1) {
                CHECK(rows < ARRAY_SIZE(row_frames));
                CHECK_EQ_INT(accepted, row_frames[rows]);
                CHECK_EQ_INT(m_Verdict.row, rows);
                rows++;
            }
            const VERDICT unchanged = m_Verdict;
            CHECK_EQ_INT(Verdict_Update(&sample), done);
            CHECK(memcmp(&unchanged, &m_Verdict, sizeof(unchanged)) == 0);
        }
        CHECK(done);
        CHECK_EQ_INT(rows, 9);
        CHECK_EQ_INT(accepted, 1599);
        CHECK_EQ_INT(g_VSync.count1, 88);
        CHECK_EQ_INT(g_VSync.count2, (uint16_t)(65530u + accepted));
        CHECK_EQ_INT(m_Palette.tone, 0);
        CHECK_EQ_INT(g_Video.drawing_page, 0);
        CHECK_EQ_INT(g_Video.visible_page, 0);
        CHECK_EQ_INT(g_Grcg->mode, 0);
        CHECK(memcmp(&session, &fixture->session, sizeof(session)) == 0);
        CHECK(Verdict_Update(nullptr));
        Arena_Free(&fixture->arena);
        Mem_Free(fixture);
    }
}

TEST(
    verdict_confirmation_preserves_two_frame_pairs_and_ignores_unmapped_wait_keys)
{
    FIXTURE *const fixture = Mem_Alloc(sizeof(*fixture));
    M_Init(fixture);
    TH02_INPUT sample = { .shot = true };
    CHECK(!Verdict_Update(&sample));
    while (m_Verdict.phase != VERDICT_CONFIRM) {
        VSyncIrq_VSync();
        CHECK(!Verdict_Update(&sample));
    }
    for (size_t i = 0; i < 6; i++) {
        VSyncIrq_VSync();
        CHECK(!Verdict_Update(&sample));
        CHECK_EQ_INT(g_VSync.count1, i % 2 == 0 ? 1 : 0);
        CHECK_EQ_INT(m_Verdict.wait.key.phase, KEY_WAIT_RELEASE);
    }
    sample = (TH02_INPUT) { .space = true, .up = true, .q = true };
    for (size_t i = 0; i < 12; i++) {
        VSyncIrq_VSync();
        CHECK(!Verdict_Update(&sample));
        CHECK_EQ_INT(m_Verdict.phase, VERDICT_CONFIRM);
    }
    CHECK_EQ_INT(m_Verdict.wait.key.phase, KEY_WAIT_PRESS);
    sample.extra_group_0 = true;
    VSyncIrq_VSync();
    CHECK(!Verdict_Update(&sample));
    CHECK_EQ_INT(m_Verdict.phase, VERDICT_CONFIRM);
    VSyncIrq_VSync();
    CHECK(!Verdict_Update(&sample));
    CHECK_EQ_INT(m_Verdict.phase, VERDICT_FADE_OUT);
    CHECK_EQ_INT(g_VSync.count1, 2);
    Arena_Free(&fixture->arena);
    Mem_Free(fixture);
}
