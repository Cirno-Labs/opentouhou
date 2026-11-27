#include "th02_palette_fixture.h"
#include "th02_clip_fixture.h"
#include "../../src/th02/grcg.c"
#include "th02/ending/render.h"
#include "th02/ending/text.h"

#include "th02_sprites.h"
#include "test.h"

static void M_Fill(void)
{
    for (size_t page = 0; page < 2; page++) {
        for (size_t i = 0; i < VIDEO_PAGE_SIZE; i++) {
            g_Video.pages[page][i] =
                (uint8_t)((i * 7 + i / 640 + page * 3) & 15);
        }
    }
}

TEST(staging_can_keep_the_previous_palette_until_the_credits_change)
{
    Video_Init(&g_HeapAllocator);
    M_Fill();
    g_Video.visible_page = 0;
    Video_SetSplit(200, 1234, 5678);
    TestClip_Set(0, 0, 639, 399);
    Grcg_Init();
    memcpy(
        m_Grcg.tiles, (const uint8_t[4]) { 1, 2, 3, 4 }, sizeof(m_Grcg.tiles));

    ENDING_IMAGES images = {};
    PI_IMAGE *const image = &images.images[ENDING_IMAGE_CREDITS_2];
    image->width = 640;
    image->height = 400;
    image->pixels = Mem_Alloc(VIDEO_VISIBLE_SIZE);
    for (size_t i = 0; i < VIDEO_VISIBLE_SIZE; i++) {
        image->pixels[i] = (uint8_t)(i % 16);
    }
    for (size_t i = 0; i < 16; i++) {
        image->palette[i] = (GFX_COLOR) { i * 17, 255 - i * 17, 0, 255 };
    }

    Palette_Init();
    m_Palette.tone = 76;
    const PALETTE_STATE before = m_Palette;
    GFX_COLOR dac[16];
    memcpy(dac, g_Video.palette, sizeof(dac));
    EndingRender_Stage(&images, ENDING_IMAGE_CREDITS_2, false);
    CHECK(memcmp(&before, &m_Palette, sizeof(before)) == 0);
    CHECK(memcmp(dac, g_Video.palette, sizeof(dac)) == 0);
    CHECK(memcmp(g_Video.pages[1], image->pixels, VIDEO_VISIBLE_SIZE) == 0);
    for (size_t i = 0; i < VIDEO_PAGE_SIZE; i++) {
        CHECK_EQ_INT(g_Video.pages[0][i], (i * 7 + i / 640) & 15);
        if (i >= VIDEO_VISIBLE_SIZE) {
            CHECK_EQ_INT(g_Video.pages[1][i], (i * 7 + i / 640 + 3) & 15);
        }
    }
    EndingRender_Stage(&images, ENDING_IMAGE_CREDITS_2, true);
    for (size_t i = 0; i < 16; i++) {
        CHECK_EQ_INT(m_Palette.colors[i][0], i * 17);
        CHECK_EQ_INT(m_Palette.colors[i][1], 255 - i * 17);
        CHECK_EQ_INT(m_Palette.colors[i][2], 0);
    }
    CHECK_EQ_INT(m_Palette.tone, 76);
    CHECK_EQ_INT(g_Video.drawing_page, 1);
    CHECK_EQ_INT(g_Video.visible_page, 0);
    CHECK_EQ_INT(g_Video.first_lines, 200);
    CHECK_EQ_INT(g_Video.first_address, 1234);
    CHECK_EQ_INT(g_Video.second_address, 5678);
    CHECK(memcmp(m_Grcg.tiles, (uint8_t[]) { 1, 2, 3, 4 }, 4) == 0);
    Mem_Free(image->pixels);
    Mem_Free(g_Video.pages[0]);
}

TEST(quarter_and_row_copies_read_mutable_page_one_and_leave_latched_tiles)
{
    Video_Init(&g_HeapAllocator);
    M_Fill();
    Video_SetSplit(200, 1234, 5678);
    TestClip_Set(0, 350, 15, 399);

    Grcg_Init();

    uint8_t *const expected = Mem_Alloc(VIDEO_PAGE_SIZE * 2);
    memcpy(expected, g_Video.pages[0], VIDEO_PAGE_SIZE * 2);
    GFX_COLOR dac[16];
    memcpy(dac, g_Video.palette, sizeof(dac));
    for (size_t step = 0; step < 108; step++) {
        const uint8_t quarter = (uint8_t)(step % 4);
        const int16_t rows = step < 8 ? 200 : step < 104 ? 2 : 0;
        const int16_t source_top = step < 8 ? 0 : (int16_t)((step * 7) % 199);
        const int16_t left = step % 2 == 0 ? 160 : 32;
        const int16_t top = 100;
        g_Video.drawing_page = g_Video.visible_page = (uint8_t)(step % 2);
        Grcg_SetColor(13);
        uint8_t *const source_page = expected + VIDEO_PAGE_SIZE;
        const size_t source = ((quarter / 2) * 200 + source_top) * VIDEO_WIDTH
            + (quarter % 2) * 320;
        source_page[source] ^= 15;
        g_Video.pages[1][source] ^= 15;
        for (int16_t row = 0; row < rows; row++) {
            memcpy(
                expected + (top + row) * VIDEO_WIDTH + left,
                source_page + source + row * VIDEO_WIDTH, 320);
        }
        EndingRender_Copy(left, top, quarter, source_top, rows);
        CHECK(memcmp(expected, g_Video.pages[0], VIDEO_PAGE_SIZE * 2) == 0);
        CHECK_EQ_INT(m_Grcg.mode, 0);
        for (uint8_t plane = 0; plane < 4; plane++) {
            uint8_t bits = (13 & (1 << plane)) != 0 ? 255 : 0;
            if (rows != 0) {
                bits = 0;
                for (size_t bit = 0; bit < 8; bit++) {
                    const size_t pixel =
                        source + (rows - 1) * VIDEO_WIDTH + 304 + bit;
                    if ((source_page[pixel] & (1 << plane)) != 0) {
                        bits |= (uint8_t)(0x80 >> bit);
                    }
                }
            }
            CHECK_EQ_INT(m_Grcg.tiles[plane], bits);
        }
        CHECK_EQ_INT(g_Video.drawing_page, rows == 0 ? step % 2 : 0);
        CHECK_EQ_INT(g_Video.visible_page, step % 2);
        CHECK_EQ_INT(g_Video.first_lines, 200);
        CHECK_EQ_INT(g_Video.first_address, 1234);
        CHECK_EQ_INT(g_Video.second_address, 5678);
        CHECK(memcmp(dac, g_Video.palette, sizeof(dac)) == 0);
    }
    Mem_Free(expected);
    Mem_Free(g_Video.pages[0]);
}

TEST(verdict_score_suppresses_zero_and_skill_keeps_the_77_title)
{
    Video_Init(&g_HeapAllocator);
    TestClip_Set(0, 0, 639, 399);
    Grcg_Init();

    ENDING_LABELS labels = {};
    const uint16_t mask[16] = { 0x8000 };
    const DRAW_TEXT_GLYPH glyph = { mask, true };
    const TRAM_CHARACTER character = { .source_end = 2 };
    DRAW_TEXT titles[12] = {};
    uint16_t title_masks[12][16] = {};
    DRAW_TEXT_GLYPH title_glyphs[12];
    for (size_t i = 0; i < ENDING_LABEL_COUNT; i++) {
        labels.text[i].glyphs = &glyph;
        labels.text[i].count = 1;
        labels.text[i].cells.characters = &character;
    }
    for (size_t i = 0; i < 12; i++) {
        title_masks[i][0] = (uint16_t)(0x8000u >> i);
        title_glyphs[i] = (DRAW_TEXT_GLYPH) { title_masks[i], true };
        titles[i].glyphs = &title_glyphs[i];
        titles[i].count = 1;
        titles[i].cells.characters = &character;
    }
    const ENDING_TEXT_SCRIPT script = { titles, 12 };
    GAIJI gaiji = {};
    for (size_t i = 0; i < 256; i++) {
        gaiji.rows[i][0] = (uint16_t)((i << 8) | i);
    }
    memset(gaiji.rows[0xcf], 0, sizeof(gaiji.rows[0xcf]));
    TH02_SESSION session = {};
    EndingRender_VerdictRow(&labels, &script, &gaiji, &session, 1, 15);
    for (size_t x = 192; x < 320; x++) {
        CHECK_EQ_INT(g_Video.pages[0][128 * 640 + x], 0);
    }
    session.score = -1;
    EndingRender_VerdictRow(&labels, &script, &gaiji, &session, 1, 15);
    CHECK_EQ_INT(Video_ReadPlaneByte(0, 0, 128 * 80 + 304 / 8), 0x9f);
    const int16_t skills[] = { INT16_MIN, 0,  9,  10, 69,  70,       76,
                               77,        78, 80, 99, 100, INT16_MAX };
    const uint8_t choices[] = { 11, 11, 11, 10, 5, 4, 4, 3, 4, 2, 1, 0, 0 };
    for (size_t i = 0; i < ARRAY_SIZE(skills); i++) {
        memset(g_Video.pages[0], 0, VIDEO_PAGE_SIZE);
        session.skill = skills[i];
        const TH02_SESSION before = session;
        EndingRender_VerdictRow(&labels, &script, &gaiji, &session, 6, 15);
        CHECK(memcmp(&session, &before, sizeof(session)) == 0);
        for (size_t x = 240; x < 256; x++) {
            CHECK_EQ_INT(
                g_Video.pages[0][288 * 640 + x],
                x == (size_t)(240 + choices[i]) ? 15 : 0);
        }
    }
    Mem_Free(g_Video.pages[0]);
}

TEST(reveal_masks_copy_native_bit_order_from_verdict_sheet)
{
    VFS *const vfs = Th02Test_Sprites();
    ENDING_REVEAL_MASKS masks;
    CHECK(IS_OK(EndingRender_LoadRevealMasks(vfs, &masks)));
    CHECK_EQ_INT(masks.rows[0][0], 0xaaaa);
    CHECK_EQ_INT(masks.rows[0][1], 0x5555);
    CHECK_EQ_INT(masks.rows[2][0], 0xcccc);
    Vfs_Destroy(vfs);
}

TEST(missing_verdict_sheet_leaves_masks_empty)
{
    VFS *const vfs = Vfs_Create(&g_HeapAllocator);
    ENDING_REVEAL_MASKS masks;
    memset(&masks, 0xa5, sizeof(masks));
    CHECK(!Result_Ignore(EndingRender_LoadRevealMasks(vfs, &masks)));
    const ENDING_REVEAL_MASKS empty = {};
    CHECK(memcmp(&masks, &empty, sizeof(masks)) == 0);
    Vfs_Destroy(vfs);
}

TEST(reveal_reads_live_page_one_and_preserves_pixels_outside_each_mask)
{
    Video_Init(&g_HeapAllocator);
    Video_ShowPage(1);
    Video_SetSplit(123, 456, 789);
    TestClip_Set(0, 0, 0, 0);
    Grcg_Init();
    Grcg_SetPlanes(1);
    memcpy(
        m_Grcg.tiles, (const uint8_t[4]) { 1, 2, 3, 4 }, sizeof(m_Grcg.tiles));

    ENDING_REVEAL_MASKS masks;
    for (size_t y = 0; y < 16; y++) {
        masks.rows[0][y] = 0xaaaa;
        masks.rows[1][y] = 0x4444;
        masks.rows[2][y] = 0x1111;
    }
    GFX_COLOR before_dac[16];
    memcpy(before_dac, g_Video.palette, sizeof(before_dac));
    ENDING_REVEAL reveal;
    EndingRender_BeginReveal(&reveal, &masks, 608, 384, 2);
    for (size_t step = 0; step < ENDING_REVEAL_STEPS; step++) {
        memset(g_Video.pages[1], (int)step + 1, VIDEO_PAGE_SIZE);
        Video_AccessPage(1);
        CHECK(!EndingRender_StepReveal(&reveal));
        for (size_t i = 0; i < VIDEO_PAGE_SIZE; i++) {
            const size_t x = i % VIDEO_WIDTH;
            const size_t y = i / VIDEO_WIDTH;
            uint8_t expected = 0;
            if (x >= 608 && y >= 384 && y < 400) {
                if (x % 2 == 0) {
                    expected = 1;
                } else if (x % 4 == 1 && step >= 1) {
                    expected = 2;
                } else if (x % 4 == 3 && step >= 2) {
                    expected = 3;
                }
            }
            CHECK_EQ_INT(g_Video.pages[0][i], expected);
            CHECK_EQ_INT(g_Video.pages[1][i], step + 1);
        }
        CHECK_EQ_INT(g_Video.drawing_page, 0);
        CHECK_EQ_INT(g_Video.visible_page, 1);
        CHECK_EQ_INT(g_Video.first_lines, 123);
        CHECK_EQ_INT(g_Video.first_address, 456);
        CHECK_EQ_INT(g_Video.second_address, 789);
        CHECK_EQ_INT(m_Grcg.mode, 0);
        for (size_t plane = 0; plane < 4; plane++) {
            CHECK_EQ_INT(m_Grcg.tiles[plane], 0);
        }
        CHECK(memcmp(g_Video.palette, before_dac, sizeof(before_dac)) == 0);
    }
    Grcg_SetColor(7);
    Grcg_SetPlanes(1);
    Video_AccessPage(1);
    CHECK(EndingRender_StepReveal(&reveal));
    CHECK(EndingRender_StepReveal(&reveal));
    CHECK_EQ_INT(m_Grcg.mode, 0xce);
    CHECK_EQ_INT(g_Video.drawing_page, 1);
    Mem_Free(g_Video.pages[0]);
}

TEST(empty_reveal_keeps_pages_and_charger_but_still_has_three_waits)
{
    Video_Init(&g_HeapAllocator);
    Video_AccessPage(1);
    TestClip_Set(0, 0, 639, 399);
    Grcg_Init();
    Grcg_SetPlanes(1);
    memcpy(
        m_Grcg.tiles, (const uint8_t[4]) { 1, 2, 3, 4 }, sizeof(m_Grcg.tiles));
    const GRCG before = m_Grcg;

    const ENDING_REVEAL_MASKS masks = {};
    ENDING_REVEAL reveal;
    EndingRender_BeginReveal(&reveal, &masks, 640, 384, 0);
    for (size_t step = 0; step < ENDING_REVEAL_STEPS; step++) {
        CHECK(!EndingRender_StepReveal(&reveal));
        CHECK_EQ_INT(g_Video.drawing_page, 1);
        CHECK(memcmp(&m_Grcg, &before, sizeof(m_Grcg)) == 0);
    }
    CHECK(EndingRender_StepReveal(&reveal));
    CHECK(memcmp(&m_Grcg, &before, sizeof(m_Grcg)) == 0);
    Mem_Free(g_Video.pages[0]);
}

TEST(rotation_finishes_after_twenty_waits_and_keeps_the_live_page_and_clip)
{
    Video_Init(&g_HeapAllocator);
    M_Fill();
    Video_AccessPage(1);
    Video_ShowPage(0);
    Video_SetSplit(123, 456, 789);
    TestClip_Set(0, 0, 639, 399);
    Grcg_Init();
    Grcg_SetPlanes(1);
    memcpy(
        m_Grcg.tiles, (const uint8_t[4]) { 1, 2, 3, 4 }, sizeof(m_Grcg.tiles));

    Palette_Init();
    m_Palette.tone = 73;
    const PALETTE_STATE before = m_Palette;
    GFX_COLOR dac[16];
    memcpy(dac, g_Video.palette, sizeof(dac));
    ENDING_ROTATION rotation;
    EndingRender_BeginRotation(&rotation, 252, 41);
    CHECK_EQ_INT(g_Video.pages[1][0], 3);
    CHECK_EQ_INT(m_Grcg.mode, 0xce);
    for (size_t step = 0; step < 20; step++) {
        CHECK(!EndingRender_StepRotation(&rotation));
        CHECK_EQ_INT(m_Grcg.mode, 0xc0);
        CHECK(
            memcmp(m_Palette.colors, before.colors, sizeof(m_Palette.colors))
            == 0);
        CHECK_EQ_INT(m_Palette.tone, 73);
        CHECK(memcmp(dac, g_Video.palette, sizeof(dac)) == 0);
    }
    CHECK(EndingRender_StepRotation(&rotation));
    CHECK_EQ_INT(m_Palette.tone, 200);
    CHECK_EQ_INT(m_Grcg.mode, 0);
    for (size_t plane = 0; plane < 4; plane++) {
        CHECK_EQ_INT(m_Grcg.tiles[plane], 255);
    }
    for (size_t y = 0; y < VIDEO_HEIGHT; y++) {
        for (size_t x = 0; x <= 384; x++) {
            const bool edge = ((x == 28 || x == 356) && y >= 96 && y <= 304)
                || ((y == 96 || y == 304) && x >= 28 && x <= 356);
            CHECK_EQ_INT(g_Video.pages[1][y * VIDEO_WIDTH + x], edge ? 15 : 0);
        }
    }
    for (size_t i = 0; i < VIDEO_PAGE_SIZE; i++) {
        CHECK_EQ_INT(g_Video.pages[0][i], (i * 7 + i / 640) & 15);
        if (i >= VIDEO_VISIBLE_SIZE) {
            CHECK_EQ_INT(g_Video.pages[1][i], (i * 7 + i / 640 + 3) & 15);
        }
    }
    CHECK_EQ_INT(g_Video.drawing_page, 1);
    CHECK_EQ_INT(g_Video.visible_page, 0);
    CHECK_EQ_INT(g_Video.first_lines, 123);
    CHECK_EQ_INT(g_Video.first_address, 456);
    CHECK_EQ_INT(g_Video.second_address, 789);
    Grcg_SetColor(7);
    Grcg_SetPlanes(1);
    m_Palette.tone = 12;
    CHECK(EndingRender_StepRotation(&rotation));
    CHECK_EQ_INT(m_Grcg.mode, 0xce);
    CHECK_EQ_INT(m_Palette.tone, 12);
    Mem_Free(g_Video.pages[0]);
}
