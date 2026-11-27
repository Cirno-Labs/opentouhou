#include "tests/test.h"
#include "tests/th02/th02_clip_fixture.h"
#include "tests/th02/th02_palette_fixture.h"
#include "th02/ending/credits.c"

#include "core/base/memory.h"

#include <string.h>

typedef struct {
    ENDING_ASSETS assets;
    DRAW_TEXT_GLYPH glyph;
    TRAM_CHARACTER character;
    uint16_t rows[16];
} FIXTURE;

static void M_Init(FIXTURE *const f)
{
    memset(&m_Palette, 0, sizeof(m_Palette));
    Grcg_Init();
    memset(&g_VSync, 0, sizeof(g_VSync));
    memset(f, 0, sizeof(*f));
    Video_Init(&g_HeapAllocator);
    Clip_Reset();
    Palette_Init();
    VSyncIrq_BeginProgram();
    g_VSync.count1 = 65534;
    g_VSync.count2 = 65530;
    PatternBank_Init(&f->assets.title, &g_HeapAllocator);
    uint8_t pixels[16 * 32];
    for (uint16_t i = 0; i < 55; i++) {
        memset(pixels, (int)(i / 5 + 1), sizeof(pixels));
        CHECK(Result_Ignore(
            PatternBank_Put(&f->assets.title, i, 16, 32, pixels, 0)));
    }
    f->rows[0] = 0x8000;
    f->glyph = (DRAW_TEXT_GLYPH) { f->rows, true };
    f->character.source_end = 2;
    for (size_t i = 0; i < ENDING_LABEL_COUNT; i++) {
        f->assets.labels.text[i].glyphs = &f->glyph;
        f->assets.labels.text[i].count = 1;
        f->assets.labels.text[i].cells.characters = &f->character;
    }
    f->assets.images.allocator = g_HeapAllocator;
    for (size_t i = ENDING_IMAGE_CREDITS_1; i <= ENDING_IMAGE_CREDITS_3; i++) {
        PI_IMAGE *const image = &f->assets.images.images[i];
        image->width = 640;
        image->height = 400;
        image->pixels = Mem_Alloc(VIDEO_VISIBLE_SIZE);
        for (size_t y = 0; y < 400; y++) {
            for (size_t x = 0; x < 640; x++) {
                image->pixels[y * 640 + x] =
                    (uint8_t)(1 + i + x / 320 + y / 200 * 2);
            }
        }
    }
    Credits_Begin(&f->assets);
    CHECK(m_Credits.assets == &f->assets);
}

static void M_Free(FIXTURE *const f)
{
    EndingImages_Free(&f->assets.images);
    PatternBank_Reset(&f->assets.title);
    Mem_Free(g_Video.pages[0]);
}

TEST(credits_complete_with_music_or_fallback)
{
    for (size_t mode = 0; mode < 2; mode++) {
        FIXTURE f;
        M_Init(&f);
        const bool music = mode != 0;
        const TH02_INPUT input = { .shot = true };
        CHECK_EQ_INT(g_VSync.count1, 65534);
        CHECK(!Credits_Update(&input, music, UINT16_MAX));
        CHECK_EQ_INT(g_VSync.count1, 0);
        size_t accepted = 0;
        bool done = false;
        while (!done && accepted < 2200) {
            VSyncIrq_VSync();
            accepted++;
            done = Credits_Update(&input, music, UINT16_MAX);
            if (!done) {
                const CREDITS before = m_Credits;
                const uint16_t counter = g_VSync.count1;
                CHECK(!Credits_Update(&input, !music, 0));
                CHECK(memcmp(&before, &m_Credits, sizeof(before)) == 0);
                CHECK_EQ_INT(g_VSync.count1, counter);
            }
        }
        CHECK(done);
        CHECK_EQ_INT(accepted, music ? 598 : 2083);
        CHECK_EQ_INT(g_VSync.count2, (uint16_t)(65530 + accepted));
        CHECK_EQ_INT(m_Credits.image, 11);
        CHECK_EQ_INT(m_Credits.title_top, 368);
        CHECK_EQ_INT(g_Video.drawing_page, 0);
        CHECK_EQ_INT(g_Video.visible_page, 0);
        CHECK_EQ_INT(g_Grcg->mode, 0);
        CHECK_EQ_INT(m_Palette.tone, 0);
        for (size_t i = 0; i < 16; i++) {
            CHECK_EQ_INT(g_Video.palette[i].red, 0);
            CHECK_EQ_INT(g_Video.palette[i].green, 0);
            CHECK_EQ_INT(g_Video.palette[i].blue, 0);
        }
        CHECK_EQ_INT(g_Video.pages[0][100 * 640 + 32], 11);
        CHECK_EQ_INT(g_Video.pages[1][0], 8);
        CHECK(Credits_Update(nullptr, false, 0));
        M_Free(&f);
    }
}

TEST(active_music_polls_only_after_a_frame_and_fixes_each_gate_mode)
{
    FIXTURE f;
    M_Init(&f);
    const TH02_INPUT input = {};
    CHECK(!Credits_Update(&input, true, 5));
    for (size_t i = 0; i < 9; i++) {
        VSyncIrq_VSync();
        CHECK(!Credits_Update(&input, false, 5));
        CHECK_EQ_INT(m_Credits.phase, CREDITS_MEASURE);
        CHECK_EQ_INT(g_VSync.count1, 0);
    }
    CHECK(!Credits_Update(&input, false, 6));
    CHECK_EQ_INT(m_Credits.title_frame, 0);
    VSyncIrq_VSync();
    CHECK(!Credits_Update(&input, false, 6));
    CHECK_EQ_INT(m_Credits.title_frame, 1);
    CHECK_EQ_INT(g_Video.pages[0][184 * 640 + 432], 1);
    M_Free(&f);
}
