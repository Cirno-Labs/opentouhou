#include "tests/test.h"
#include "tests/th02/th02_clip_fixture.h"
#include "tests/th02/th02_tram_fixture.h"
#include "th02/bomb/private.h"
#include "th02/grcg_private.h"

#include "core/base/arena.h"

#include <string.h>

typedef struct {
    ARENA arena;

    BOMB_ASSETS assets;

    int16_t scroll;
} FIXTURE;

static void M_Init(FIXTURE *const fixture)
{
    Grcg_Init();
    Tram_Init();
    memset(fixture, 0, sizeof(*fixture));
    Arena_Init(&fixture->arena, "Bomb rendering", 1024 * 1024);
    Video_Init(Arena_Allocator(&fixture->arena));
    Video_AccessPage(1);
    memset(g_Video.pages[0], 6, VIDEO_PAGE_SIZE);
    memset(g_Video.pages[1], 9, VIDEO_PAGE_SIZE);
    TestClip_Set(0, 0, 1, 1);
    Grcg_SetColor(14);
}

static void M_Free(FIXTURE *const fixture)
{
    Arena_Free(&fixture->arena);
}

TEST(circle_shifts_across_bytes_rolls_rows_and_ignores_clip)
{
    FIXTURE fixture;
    M_Init(&fixture);
    fixture.scroll = 383;
    fixture.assets.particles[4][0] = 0x81;
    fixture.assets.particles[4][1] = 0x40;
    BombRender_CirclePoint(&fixture.assets, fixture.scroll, 33, 16);
    CHECK_EQ_INT(g_Video.pages[1][399 * 640 + 33], 14);
    CHECK_EQ_INT(g_Video.pages[1][399 * 640 + 40], 14);
    CHECK_EQ_INT(g_Video.pages[1][34], 14);
    CHECK_EQ_INT(g_Video.pages[1][32], 9);
    CHECK_EQ_INT(g_Video.pages[0][399 * 640 + 33], 6);
    CHECK_EQ_INT(p_Grcg->mode, 0xc0);
    CHECK_EQ_INT(p_Grcg->tiles[0], 0);
    CHECK_EQ_INT(g_Video.drawing_page, 1);
    BombRender_CirclePoint(&fixture.assets, fixture.scroll, 24, 16);
    BombRender_CirclePoint(&fixture.assets, fixture.scroll, 33, 8);
    CHECK_EQ_INT(g_Video.pages[1][399 * 640 + 24], 9);
    M_Free(&fixture);
}

TEST(particle_and_smear_keep_byte_alignment_plane_masks_and_short_tail)
{
    FIXTURE fixture;
    M_Init(&fixture);
    p_Grcg->mode = 0xc1;
    fixture.assets.particles[2][0] = 0x80;
    BombRender_Particle(&fixture.assets, fixture.scroll, 39, 16, 2);
    CHECK_EQ_INT(g_Video.pages[1][16 * 640 + 32], 15);
    CHECK_EQ_INT(g_Video.pages[1][16 * 640 + 39], 9);
    Grcg_SetColor(14);
    fixture.assets.particles[4][4] = 0x80;
    fixture.assets.particles[4][5] = 0x40;
    fixture.assets.particles[4][6] = 0x20;
    fixture.assets.particles[4][7] = 0xff;
    BombRender_Smear(&fixture.assets, fixture.scroll, 48, 16);
    CHECK_EQ_INT(g_Video.pages[1][16 * 640 + 48], 14);
    CHECK_EQ_INT(g_Video.pages[1][16 * 640 + 55], 14);
    CHECK_EQ_INT(g_Video.pages[1][17 * 640 + 48], 14);
    CHECK_EQ_INT(g_Video.pages[1][18 * 640 + 49], 14);
    CHECK_EQ_INT(g_Video.pages[1][19 * 640 + 50], 14);
    CHECK_EQ_INT(g_Video.pages[1][20 * 640 + 48], 9);
    M_Free(&fixture);
}

TEST(transition_reaches_row_399_and_text_keeps_outer_cells_and_sheet_padding)
{
    FIXTURE fixture;
    M_Init(&fixture);
    uint8_t frame[72] = {};
    frame[71] = 1;
    BombRender_Transition(fixture.scroll, frame);
    CHECK_EQ_INT(g_Video.pages[1][399 * 640 + 415], 14);
    CHECK_EQ_INT(g_Video.pages[1][383 * 640 + 415], 9);
    CHECK_EQ_INT(g_Video.pages[1][399 * 640 + 416], 9);
    fixture.assets.shot_type = 1;
    memset(p_Tram, 0xa5, sizeof(*p_Tram));
    memset(fixture.assets.text[17], 0, 288);
    fixture.assets.text[17][0] = 0x10;
    fixture.assets.text[17][275] = 0x01;
    memset(fixture.assets.text[17] + 276, 0xff, 12);
    BombRender_Text(&fixture.assets, 17);
    CHECK_EQ_INT(p_Tram->codes[84], 0x20);
    CHECK_EQ_INT(p_Tram->attributes[84], 0x45);
    CHECK_EQ_INT(p_Tram->attributes[85], 0x45);
    CHECK_EQ_INT(p_Tram->attributes[86], 0xe1);
    CHECK_EQ_INT(p_Tram->attributes[23 * 80 + 50], 0x45);
    CHECK_EQ_INT(p_Tram->attributes[23 * 80 + 51], 0x45);
    CHECK_EQ_INT(p_Tram->codes[23 * 80 + 52], 0xa5a5);
    CHECK_EQ_INT(p_Tram->codes[2000], 0xa5a5);
    M_Free(&fixture);
}

TEST(illustration_rolls_opaque_pixels_without_using_palette_or_color_key)
{
    FIXTURE fixture;
    M_Init(&fixture);
    fixture.scroll = 399;
    TestClip_Set(100, 0, 101, 399);
    const uint8_t pixels[16 * 2] = { 0, 1, 2,  3,  4,  5,  6,  7,
                                     8, 9, 10, 11, 12, 13, 14, 15 };
    fixture.assets.illustration = (PI_IMAGE) {
        .width = 16,
        .height = 2,
        .transparent = 0,
        .pixels = (uint8_t *)pixels,
    };
    Grcg_Off();
    BombRender_Illustration(&fixture.assets, fixture.scroll, 39, 0);
    CHECK_EQ_INT(g_Video.pages[1][399 * 640 + 32], 0);
    CHECK_EQ_INT(g_Video.pages[1][399 * 640 + 47], 15);
    CHECK_EQ_INT(g_Video.pages[1][32], 0);
    CHECK_EQ_INT(p_Grcg->mode, 0);
    CHECK_EQ_INT(p_Grcg->tiles[0], 0);
    M_Free(&fixture);
}
