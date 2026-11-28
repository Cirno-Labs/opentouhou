#include "tests/test.h"
#include "th01/shape.h"
#include "th01/sprite.h"

#include "core/base/memory.h"

#include <string.h>

static uint8_t m_Expected[VIDEO_PAGE_SIZE];

static void M_Video(void)
{
    Mem_Free(g_Video->pages[0]);
    Video_Init(&g_HeapAllocator);
    for (int32_t i = 0; i < VIDEO_PAGE_SIZE; i++) {
        g_Video->pages[0][i] = 7;
        g_Video->pages[1][i] = i % 16;
        m_Expected[i] = 7;
    }
}

TEST(ptn_quarters_restore_and_row_wrapping)
{
    uint8_t pixels[1024];
    for (int32_t i = 0; i < 1024; i++) {
        pixels[i] = i % 17 == 0 ? 15 : (i / 16) % 15;
    }
    const PTN sprites = { .count = 1, .pixels = pixels };
    const int16_t positions[] = { -8, -4, 0, 4, 608, 612, 616, 636 };
    for (int32_t q = -1; q < 4; q++) {
        for (size_t p = 0; p < ARRAY_SIZE(positions); p++) {
            M_Video();
            const int32_t width = q < 0 ? 32 : 16;
            const int32_t x0 = q < 0 ? 0 : (q % 2) * 16;
            const int32_t y0 = q < 0 ? 0 : (q / 2) * 16;
            const int32_t byte_x =
                positions[p] >= 0 ? positions[p] / 8 : (positions[p] - 7) / 8;
            for (int32_t y = 0; y < width; y++) {
                for (int32_t x = 0; x < width; x++) {
                    const uint8_t pixel = pixels[(y + y0) * 32 + x + x0];
                    const int32_t offset = (384 + y) * 640 + byte_x * 8 + x;
                    if (offset >= 0 && offset < VIDEO_PAGE_SIZE
                        && pixel != 15) {
                        m_Expected[offset] = pixel;
                    }
                }
            }
            Sprite_PtnPut(&sprites, 0, q, positions[p], 384);
            CHECK(memcmp(g_Video->pages[0], m_Expected, VIDEO_PAGE_SIZE) == 0);
            Sprite_PtnErase(&sprites, 0, q, positions[p], 384);
            for (int32_t i = 0; i < VIDEO_PAGE_SIZE; i++) {
                if (m_Expected[i] != 7) {
                    CHECK_EQ_INT(g_Video->pages[0][i], g_Video->pages[1][i]);
                }
            }
        }
    }
}

TEST(rect_restore_covers_extra_word_and_wraps_rows)
{
    M_Video();
    Sprite_Restore16(636, 398, 16, 16);
    for (int32_t i = 0; i < VIDEO_PAGE_SIZE; i++) {
        bool restored = false;
        for (int32_t y = 398; y < 414; y++) {
            for (int32_t x = 624; x < 656; x++) {
                if ((((uint32_t)(y * 640 + x)) & 0x3ffffu) == (uint32_t)i) {
                    restored = true;
                }
            }
        }
        CHECK_EQ_INT(g_Video->pages[0][i], restored ? g_Video->pages[1][i] : 7);
    }
    CHECK_EQ_INT(g_Video->drawing_page, 0);
}

TEST(player_masks_raw_color_and_retains_clipping_quirk)
{
    uint8_t pixels[48 * 2];
    uint8_t coverage[48 * 2];
    memset(pixels, 14, sizeof(pixels));
    memset(coverage, 1, sizeof(coverage));
    coverage[8] = 0;
    const BOS sprites = { 48, 2, 1, pixels, coverage };
    M_Video();
    Sprite_PlayerPut(&sprites, 0, -4, 1);
    CHECK_EQ_INT(g_Video->pages[0][632], 7);
    CHECK_EQ_INT(g_Video->pages[0][640], 7);
    CHECK_EQ_INT(g_Video->pages[0][641], 14);
    Sprite_PlayerErase(&sprites, 0, -4, 1);
    CHECK_EQ_INT(g_Video->pages[0][641], g_Video->pages[1][641]);
    M_Video();
    Sprite_PlayerPut(&sprites, 0, 636, 1);
    CHECK_EQ_INT(g_Video->pages[0][1279], 14);
    CHECK_EQ_INT(g_Video->pages[0][1280], 7);
}

TEST(player_overlap_preserves_uncovered_old_pixels_only)
{
    uint8_t pixels[48 * 2];
    uint8_t coverage[48 * 2];
    memset(pixels, 3, sizeof(pixels));
    memset(coverage, 1, 48);
    memset(coverage + 48, 0, 48);
    coverage[48 + 8] = 1;
    const BOS sprites = { 48, 1, 2, pixels, coverage };
    for (int16_t delta = -8; delta <= 8; delta += 8) {
        M_Video();
        Sprite_PlayerPut(&sprites, 0, 16, 2);
        Sprite_PlayerOverlap(&sprites, 1, 0, 16 + delta, 2, 16, 2);
        for (int32_t x = 16; x < 64; x++) {
            CHECK_EQ_INT(
                g_Video->pages[0][1280 + x],
                x == 24 + delta ? 3 : g_Video->pages[1][1280 + x]);
        }
    }
}

TEST(egc_hidden_rows_and_plane_boundary_are_distinct)
{
    M_Video();
    Sprite_Restore16(0, 400, 8, 1);
    CHECK(memcmp(g_Video->pages[0], m_Expected, VIDEO_PAGE_SIZE) == 0);
    Sprite_Restore16(384, 409, 8, 1);
    for (int32_t i = 0; i < 16; i++) {
        CHECK_EQ_INT(g_Video->pages[0][i], g_Video->pages[1][i]);
    }
    CHECK_EQ_INT(g_Video->pages[0][16], 7);
}

TEST(bottom_card_splash_restores_wrapped_top_row)
{
    M_Video();
    Shape_EllipseArcErase(16, 368, 44, 44, 8, 0, 255);
    // The quarter-turn sample is (16, 412), planar byte offset 32962.
    CHECK_EQ_INT(g_Video->pages[0][1552], g_Video->pages[1][1552]);
    CHECK_EQ_INT(g_Video->pages[0][1567], g_Video->pages[1][1567]);
}
