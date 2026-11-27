#include "th02_clip_fixture.h"
#include "th02/draw.h"
#include "th02/tram.h"

#include "core/base/arena.h"

#include "test.h"

TEST(sprite_cleanup_controls_the_next_box_fill)
{
    ARENA arena;
    Arena_Init(&arena, "draw test", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));

    Clip_Reset();

    Grcg_Init();

    uint8_t data[10] = {};
    const PATTERN pattern = { .size = 0x0102,
                              .data = data,
                              .storage_size = sizeof(data) };
    Grcg_SetColor(7);
    Draw_Sprite(&pattern, 0, 0);
    CHECK_EQ_INT(g_Grcg->mode, 0);
    for (size_t plane = 0; plane < 4; plane++) {
        CHECK_EQ_INT(g_Grcg->tiles[plane], 255);
    }
    Draw_BoxFill(g_Clip, 3, 0, 3, 0);
    CHECK_EQ_INT(g_Video.pages[0][3], 1);
    CHECK_EQ_INT(g_Video.pages[0][2], 0);
    Grcg_SetColor(7);
    Draw_SpriteRect(&pattern, 0, 0);
    CHECK_EQ_INT(g_Grcg->mode, 0);
    Draw_SpritePlane(&pattern, 0, 0, 0, 0xa5ce);
    CHECK_EQ_INT(g_Grcg->mode, 0);
    for (size_t plane = 0; plane < 4; plane++) {
        CHECK_EQ_INT(g_Grcg->tiles[plane], 0xa5);
    }
    Arena_Free(&arena);
}

TEST(rejected_rectangles_and_empty_tiny_sprites_preserve_the_charger)
{
    ARENA arena;
    Arena_Init(&arena, "draw test", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));

    Clip_Reset();

    Grcg_Init();

    uint8_t data[160] = {};
    PATTERN pattern = { .size = 0x0210,
                        .data = data,
                        .storage_size = sizeof(data) };
    Grcg_SetColor(1);
    Grcg_SetPlanes(1);
    const GRCG before = *g_Grcg;
    Draw_SpriteRect(&pattern, 640, 0);
    CHECK(memcmp(g_Grcg, &before, sizeof(*g_Grcg)) == 0);
    Draw_SpriteRollTiny(&pattern, 0, 0);
    CHECK(memcmp(g_Grcg, &before, sizeof(*g_Grcg)) == 0);
    Draw_BoxFill(g_Clip, 3, 0, 3, 0);
    for (size_t x = 0; x < 16; x++) {
        CHECK_EQ_INT(g_Video.pages[0][x], x == 3 ? 1 : 0);
    }
    data[0] = 0x80;
    data[1] = 3;
    data[34] = 0x80;
    data[35] = 6;
    Draw_SpriteRollTiny(&pattern, 0, 0);
    CHECK_EQ_INT(g_Grcg->mode, 0);
    CHECK_EQ_INT(g_Grcg->tiles[0], 0);
    CHECK_EQ_INT(g_Grcg->tiles[1], 255);
    CHECK_EQ_INT(g_Grcg->tiles[2], 255);
    CHECK_EQ_INT(g_Grcg->tiles[3], 0);
    Arena_Free(&arena);
}

TEST(gaiji_masks_replace_only_covered_pixels_and_preserve_the_other_page)
{
    ARENA arena;
    Arena_Init(&arena, "gaiji drawing test", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));

    Clip_Reset();

    Grcg_Init();

    GAIJI bank = {};
    bank.rows[5][0] = 0x8101;
    bank.rows[5][15] = 0x8000;
    memset(g_Video.pages[0], 7, VIDEO_PAGE_SIZE);
    memset(g_Video.pages[1], 12, VIDEO_PAGE_SIZE);
    Draw_Gaiji(&bank, 3, 2, 5, 0xf0, false);
    for (size_t pixel = 0; pixel < VIDEO_PAGE_SIZE; pixel++) {
        const bool covered =
            pixel == 1283 || pixel == 1290 || pixel == 1298 || pixel == 10883;
        CHECK_EQ_INT(g_Video.pages[0][pixel], covered ? 0 : 7);
        CHECK_EQ_INT(g_Video.pages[1][pixel], 12);
    }
    CHECK_EQ_INT(g_Grcg->mode, 0);
    for (size_t plane = 0; plane < 4; plane++) {
        CHECK_EQ_INT(g_Grcg->tiles[plane], 0);
    }
    Arena_Free(&arena);
}

TEST(gaiji_carry_selects_the_original_next_code_including_id_255)
{
    ARENA arena;
    Arena_Init(&arena, "gaiji carry test", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));

    Clip_Reset();

    Grcg_Init();

    GAIJI bank = {};
    bank.rows[127][0] = 0x8000;
    bank.rows[128][0] = 0x4000;
    bank.rows[255][0] = 0x2000;
    Draw_Gaiji(&bank, 0, 0, 127, 3, true);
    Draw_Gaiji(&bank, 8, 0, 255, 5, true);
    Draw_Gaiji(&bank, 16, 0, 255, 6, false);
    Draw_Gaiji(&bank, 24, 0, 127, 9, false);
    CHECK_EQ_INT(g_Video.pages[0][0], 0);
    CHECK_EQ_INT(g_Video.pages[0][1], 3);
    CHECK_EQ_INT(g_Video.pages[0][9], 5);
    CHECK_EQ_INT(g_Video.pages[0][18], 6);
    CHECK_EQ_INT(g_Video.pages[0][24], 9);
    CHECK_EQ_INT(g_Grcg->mode, 0);
    CHECK_EQ_INT(g_Grcg->tiles[0], 255);
    CHECK_EQ_INT(g_Grcg->tiles[1], 0);
    CHECK_EQ_INT(g_Grcg->tiles[2], 0);
    CHECK_EQ_INT(g_Grcg->tiles[3], 255);
    Arena_Free(&arena);
}

TEST(gaiji_strings_keep_byte_column_accumulation_and_empty_string_cleanup)
{
    ARENA arena;
    Arena_Init(&arena, "gaiji string test", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));

    Clip_Reset();

    Grcg_Init();

    GAIJI bank = {};
    bank.rows[0][0] = 0xffff;
    bank.rows[1][0] = 0x8000;
    const uint8_t ids[] = { 1, 1, 1, 0, 1 };
    Draw_GaijiString(&bank, 0, 0, -1, ids, 13);
    CHECK_EQ_INT(g_Video.pages[0][0], 13);
    CHECK_EQ_INT(g_Video.pages[0][65535], 13);
    CHECK_EQ_INT(g_Video.pages[0][65534], 13);
    CHECK_EQ_INT(g_Video.pages[0][65533], 0);
    Draw_GaijiString(&bank, 3, 1, 16, ids, 10);
    CHECK_EQ_INT(g_Video.pages[0][643], 10);
    CHECK_EQ_INT(g_Video.pages[0][659], 10);
    CHECK_EQ_INT(g_Video.pages[0][675], 10);
    Grcg_SetColor(7);
    Grcg_SetPlanes(1);
    const uint8_t empty[] = { 0 };
    Draw_GaijiString(&bank, 0, 0, 16, empty, 12);
    CHECK_EQ_INT(g_Grcg->mode, 0);
    CHECK_EQ_INT(g_Grcg->tiles[0], 0);
    CHECK_EQ_INT(g_Grcg->tiles[1], 0);
    CHECK_EQ_INT(g_Grcg->tiles[2], 255);
    CHECK_EQ_INT(g_Grcg->tiles[3], 255);
    CHECK_EQ_INT(g_Video.pages[0][0], 13);
    Arena_Free(&arena);
}

TEST(normal_text_weights_keep_original_sixteen_bit_overflow_and_holes)
{
    ARENA arena;
    Arena_Init(&arena, "normal text test", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    Grcg_Init();

    const uint16_t rows[16] = { 0xa000 };
    const DRAW_TEXT_GLYPH glyph = { rows, false };
    const uint16_t expected[] = { 0xa000, 0xe000, 0x6000, 0xe000 };
    for (uint16_t weight = 0; weight < 4; weight++) {
        memset(g_Video.pages[0], 7, VIDEO_PAGE_SIZE);
        Draw_Text(&glyph, 1, 3, 0, (weight << 4) | 2);
        for (size_t x = 0; x < 24; x++) {
            const bool covered = x >= 3 && x < 19
                && (expected[weight] & (0x8000u >> (x - 3))) != 0;
            CHECK_EQ_INT(g_Video.pages[0][x], covered ? 2 : 7);
        }
        CHECK_EQ_INT(g_Grcg->mode, 0);
        CHECK_EQ_INT(g_Grcg->tiles[0], 0);
        CHECK_EQ_INT(g_Grcg->tiles[1], 255);
        CHECK_EQ_INT(g_Grcg->tiles[2], 0);
        CHECK_EQ_INT(g_Grcg->tiles[3], 0);
    }
    Arena_Free(&arena);
}

TEST(normal_text_spacing_and_right_edge_use_the_cached_cell_width)
{
    ARENA arena;
    Arena_Init(&arena, "normal text edge test", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    Grcg_Init();

    const uint16_t rows[16] = { 0x8000 };
    const DRAW_TEXT_GLYPH glyphs[] = { { rows, false },
                                       { rows, true },
                                       { rows, false } };
    Draw_Text(glyphs, 3, 0, 0, (7 << 6) | 5);
    CHECK_EQ_INT(g_Video.pages[0][0], 5);
    CHECK_EQ_INT(g_Video.pages[0][15], 5);
    CHECK_EQ_INT(g_Video.pages[0][38], 5);
    Draw_Text(glyphs, 3, 624, 0, 6);
    CHECK_EQ_INT(g_Video.pages[0][624], 6);
    CHECK_EQ_INT(g_Video.pages[0][632], 0);
    Draw_Text(glyphs, 1, 632, 0, 9);
    CHECK_EQ_INT(g_Video.pages[0][632], 9);
    Draw_Text(glyphs + 1, 1, 625, 1, 11);
    CHECK_EQ_INT(g_Video.pages[0][640 + 625], 0);
    Draw_Text(nullptr, 0, 0, 0, 12);
    CHECK_EQ_INT(g_Grcg->mode, 0);
    CHECK_EQ_INT(g_Grcg->tiles[2], 255);
    CHECK_EQ_INT(g_Grcg->tiles[3], 255);
    Arena_Free(&arena);
}

TEST(normal_text_keeps_hidden_tail_and_negative_top_wrapping)
{
    ARENA arena;
    Arena_Init(&arena, "normal text address test", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    Grcg_Init();

    const uint16_t rows[16] = { 0x8000, 0, 0x8000 };
    const DRAW_TEXT_GLYPH glyph = { rows, true };
    g_Video.drawing_page = 1;
    Draw_Text(&glyph, 1, 4, 400, 3);
    CHECK_EQ_INT(g_Video.pages[1][VIDEO_VISIBLE_SIZE + 4], 3);
    Draw_Text(&glyph, 1, 4, -1, 6);
    CHECK_EQ_INT(g_Video.pages[1][(VIDEO_PLANE_SIZE - 80) * 8 + 4], 6);
    CHECK_EQ_INT(g_Video.pages[1][640 + 4], 6);
    CHECK_EQ_INT(g_Video.pages[0][640 + 4], 0);
    CHECK_EQ_INT(g_Video.drawing_page, 1);
    CHECK_EQ_INT(g_Video.visible_page, 0);
    Arena_Free(&arena);
}

TEST(stubbed_scene_composes_background_sprites_text_and_scanout_on_either_page)
{
    for (uint8_t page = 0; page < 2; page++) {
        ARENA arena;
        Arena_Init(&arena, "composed graphics scene", 1024 * 1024);
        Video_Init(Arena_Allocator(&arena));
        memset(g_Video.pages[1 - page], 6, VIDEO_PAGE_SIZE);

        Clip_Reset();

        Grcg_Init();

        Video_AccessPage(page);
        Video_ShowPage(page);
        Grcg_SetColor(2);
        Draw_BoxFill(g_Clip, 0, 0, 639, 399);
        Grcg_SetColor(4);
        Draw_BoxFill(g_Clip, 64, 64, 127, 127);
        Grcg_Off();

        PATTERN_BANK patterns;
        PatternBank_Init(&patterns, Arena_Allocator(&arena));
        uint8_t pixels[256];
        for (size_t y = 0; y < 16; y++) {
            for (size_t x = 0; x < 16; x++) {
                pixels[y * 16 + x] = x >= y ? 9 : 0;
            }
        }
        CHECK(Result_Ignore(PatternBank_Put(&patterns, 0, 16, 16, pixels, 0)));
        Draw_SpriteRect(&patterns.entries[0], 80, 80);
        uint16_t rows[16];
        for (size_t y = 0; y < 16; y++) {
            rows[y] = 0x8000u >> y;
        }
        const DRAW_TEXT_GLYPH glyph = { rows, true };
        Draw_Text(&glyph, 1, 88, 88, 15);
        Grcg_SetColor(3);
        Draw_BoxFill(g_Clip, 92, 92, 93, 93);
        Grcg_Off();

        uint8_t *const screen = Arena_Alloc(&arena, VIDEO_VISIBLE_SIZE);
        Video_ScrollUp(399);
        Video_Scanout(screen, 640);
        for (size_t y = 0; y < 400; y++) {
            const size_t world_y = (y + 399) % 400;
            for (size_t x = 0; x < 640; x++) {
                uint8_t expected = 2;
                if (x >= 64 && x <= 127 && world_y >= 64 && world_y <= 127) {
                    expected = 4;
                }
                if (x >= 80 && x < 96 && world_y >= 80 && world_y < 96
                    && x - 80 >= world_y - 80) {
                    expected = 9;
                }
                if (x >= 88 && x < 104 && world_y >= 88 && world_y < 104
                    && x - 88 == world_y - 88) {
                    expected = 15;
                }
                if (x >= 92 && x <= 93 && world_y >= 92 && world_y <= 93) {
                    expected = 3;
                }
                CHECK_EQ_INT(screen[y * 640 + x], expected);
            }
        }

        Tram_Init();
        GAIJI gaiji = {};
        gaiji.rows[1][0] = 0xffff;
        Tram_PutGaiji(0, 0, 1, 0xe1);
        const TRAM_FONT font = {};
        Tram_Scanout(&gaiji, &font, 0, true, screen, 640);
        for (size_t x = 0; x < 16; x++) {
            CHECK_EQ_INT(screen[x], 8);
        }
        CHECK_EQ_INT(screen[16], 0);
        CHECK_EQ_INT(screen[640], 0);
        CHECK_EQ_INT(g_Video.drawing_page, page);
        CHECK_EQ_INT(g_Video.visible_page, page);
        CHECK_EQ_INT(g_Grcg->mode, 0);
        for (size_t i = 0; i < VIDEO_PAGE_SIZE; i++) {
            CHECK_EQ_INT(g_Video.pages[1 - page][i], 6);
        }
        Arena_Free(&arena);
    }
}

TEST(circle_dots_keep_alignment_and_hidden_rows_without_clipping_or_rolling)
{
    ARENA arena;
    Arena_Init(&arena, "circle dots", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    TestClip_Set(0, 0, 0, 0);

    Grcg_SetColor(14);
    const GRCG before = *g_Grcg;

    Video_AccessPage(1);
    Video_ShowPage(0);
    for (uint8_t size = 0; size <= 5; size++) {
        for (int shift = 0; shift < 8; shift++) {
            memset(g_Video.pages[0], 5, VIDEO_PAGE_SIZE);
            memset(g_Video.pages[1], 3, VIDEO_PAGE_SIZE);
            const int left = 32 + shift;
            Draw_Dot(left, 399, size);
            for (size_t pixel = 0; pixel < VIDEO_PAGE_SIZE; pixel++) {
                const size_t x = pixel % VIDEO_WIDTH;
                const size_t y = pixel / VIDEO_WIDTH;
                const bool covered = x >= (size_t)left
                    && x < (size_t)left + size && y >= 399
                    && y < (size_t)(399 + size);
                CHECK_EQ_INT(g_Video.pages[1][pixel], covered ? 14 : 3);
                CHECK_EQ_INT(g_Video.pages[0][pixel], 5);
            }
            CHECK(memcmp(g_Grcg, &before, sizeof(before)) == 0);
            CHECK_EQ_INT(g_Video.drawing_page, 1);
            CHECK_EQ_INT(g_Video.visible_page, 0);
        }
    }
    Arena_Free(&arena);
}

TEST(circle_dot_raw_word_crosses_red_to_green_before_row_address_wrap)
{
    ARENA arena;
    Arena_Init(&arena, "circle dot alias", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    Grcg_Init();

    memset(g_Video.pages[0], 15, VIDEO_PAGE_SIZE);
    Draw_Dot(-8, 0, 1);
    CHECK_EQ_INT(Video_ReadPlaneByte(0, 0, 32767), 255);
    CHECK_EQ_INT(Video_ReadPlaneByte(0, 1, 32767), 128);
    CHECK_EQ_INT(Video_ReadPlaneByte(0, 2, 0), 0);
    CHECK_EQ_INT(Video_ReadPlaneByte(0, 3, 0), 255);
    CHECK_EQ_INT(Video_ReadPlaneByte(0, 0, 0), 255);
    memset(g_Video.pages[0], 15, VIDEO_PAGE_SIZE);
    Grcg_SetColor(2);
    Draw_Dot(-8, 0, 1);
    CHECK_EQ_INT(Video_ReadPlaneByte(0, 0, 32767), 127);
    CHECK_EQ_INT(Video_ReadPlaneByte(0, 1, 32767), 255);
    CHECK_EQ_INT(Video_ReadPlaneByte(0, 2, 32767), 127);
    CHECK_EQ_INT(Video_ReadPlaneByte(0, 2, 0), 255);
    Arena_Free(&arena);
}

TEST(circle_dot_keeps_neighbors_with_an_empty_second_mask)
{
    ARENA arena;
    Arena_Init(&arena, "circle dot masked write", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));

    Grcg_SetColor(1);
    Grcg_SetPlanes(1);

    Draw_Dot(7, 0, 1);
    for (size_t x = 0; x < 17; x++) {
        CHECK_EQ_INT(g_Video.pages[0][x], x == 7 ? 1 : 0);
    }
    CHECK_EQ_INT(g_Grcg->mode, 0xce);
    CHECK_EQ_INT(g_Grcg->tiles[0], 255);
    Arena_Free(&arena);
}

TEST(circle_outline_keeps_the_clipped_bottom_edge_rule_and_live_charger)
{
    ARENA arena;
    Arena_Init(&arena, "circle drawing test", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    TestClip_Set(32, 16, 416, 320);

    Grcg_Init();
    Grcg_SetColor(13);

    Video_AccessPage(1);
    const GRCG before_charger = *g_Grcg;
    Draw_Circle(224, 318, 0);
    CHECK_EQ_INT(g_Video.pages[1][318 * 640 + 224], 0);
    Draw_Circle(224, 318, 2);
    CHECK_EQ_INT(g_Video.pages[1][320 * 640 + 224], 13);
    CHECK_EQ_INT(g_Video.pages[1][316 * 640 + 224], 13);
    CHECK_EQ_INT(g_Video.pages[1][318 * 640 + 224], 0);
    memset(g_Video.pages[1], 0, VIDEO_PAGE_SIZE);
    TestClip_Set(224, 16, 416, 320);
    Draw_Circle(224, 318, 2);
    CHECK_EQ_INT(g_Video.pages[1][320 * 640 + 224], 0);
    CHECK_EQ_INT(g_Video.pages[1][316 * 640 + 224], 13);
    CHECK_EQ_INT(g_Video.pages[1][318 * 640 + 226], 13);
    CHECK_EQ_INT(g_Video.pages[1][318 * 640 + 222], 0);
    CHECK_EQ_INT(g_Video.pages[0][316 * 640 + 224], 0);
    CHECK_EQ_INT(g_Video.drawing_page, 1);
    CHECK_EQ_INT(g_Video.visible_page, 0);
    CHECK(memcmp(&before_charger, g_Grcg, sizeof(before_charger)) == 0);
    Arena_Free(&arena);
}

TEST(zero_radius_circle_fill_uses_a_byte_write_and_larger_discs_use_lines)
{
    ARENA arena;
    Arena_Init(&arena, "circle fill test", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    TestClip_Set(32, 0, 416, 399);

    Grcg_Init();

    memset(g_Video.pages[0], 5, VIDEO_PAGE_SIZE);
    Grcg_SetColor(10);
    Grcg_SetPlanes(15);
    Draw_CircleFill(225, 200, 0);
    for (int x = 223; x <= 232; x++) {
        CHECK_EQ_INT(g_Video.pages[0][200 * 640 + x], x == 225 ? 10 : 5);
    }
    memset(g_Video.pages[0], 5, VIDEO_PAGE_SIZE);
    Grcg_SetColor(0);
    Grcg_Off();
    Draw_CircleFill(224, 200, 0);
    CHECK_EQ_INT(g_Video.pages[0][200 * 640 + 224], 5);
    CHECK_EQ_INT(g_Video.pages[0][200 * 640 + 225], 4);
    CHECK_EQ_INT(g_Video.pages[0][200 * 640 + 232], 5);
    Grcg_SetColor(3);
    Draw_CircleFill(224, 200, 150);
    CHECK_EQ_INT(g_Video.pages[0][200 * 640 + 224], 3);
    CHECK_EQ_INT(g_Video.pages[0][50 * 640 + 224], 3);
    CHECK_EQ_INT(g_Video.pages[0][350 * 640 + 224], 3);
    CHECK_EQ_INT(g_Video.pages[0][200 * 640 + 73], 5);
    CHECK_EQ_INT(g_Video.pages[1][200 * 640 + 224], 0);
    CHECK_EQ_INT(g_Grcg->mode, 0xc0);
    Arena_Free(&arena);
}
