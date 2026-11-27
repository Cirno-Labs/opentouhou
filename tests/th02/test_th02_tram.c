#include "tests/test.h"
#include "tests/th02/th02_tram_fixture.h"
#include "th02/tram.h"

#include "core/base/memory.h"

#include <string.h>

TEST(console_clear_homes_the_software_cursor_and_keeps_hidden_cells_and_flags)
{
    static const uint8_t attributes[] = {
        TRAM_CONSOLE_DEFAULT_ATTRIBUTE,
        0x41,
        0xe5,
    };
    for (size_t mode = 0; mode < ARRAY_SIZE(attributes); mode++) {
        memset(&m_Tram, 0x99, sizeof(m_Tram));
        TRAM_CONSOLE console;
        TramConsole_Init(&console, attributes[mode]);
        CHECK(console.cursor_visible && console.system_line_visible);
        console.cursor_column = 33;
        console.cursor_row = 11;
        console.cursor_visible = console.system_line_visible = false;
        TramConsole_Clear(&console);
        CHECK_EQ_INT(console.cursor_column, 0);
        CHECK_EQ_INT(console.cursor_row, 0);
        CHECK_EQ_INT(console.attribute, attributes[mode]);
        CHECK(!console.cursor_visible && !console.system_line_visible);
        for (size_t i = 0; i < TRAM_PLANE_CELLS; i++) {
            CHECK_EQ_INT(m_Tram.codes[i], i < 2000 ? ' ' : 0x9999);
            CHECK_EQ_INT(
                m_Tram.attributes[i], i < 2000 ? attributes[mode] : 0x9999);
        }
    }
}

TEST(display_clock_matches_measured_blink_intervals_without_mutating_text)
{
    static const uint16_t transitions[] = { 30, 126, 158, 254, 286 };

    Tram_Init();
    GAIJI bank = {};
    bank.rows[1][0] = 0xffff;
    Tram_PutGaiji(8, 0, 1, 0xe3);
    Tram_PutGaiji(10, 0, 1, 0xe1);
    const TRAM saved = m_Tram;
    TRAM_DISPLAY display;
    TramDisplay_Init(&display, 2);
    display.first_cell = 8;
    const TRAM_FONT font = {};
    uint8_t *const pixels = Mem_Alloc(640 * 400);
    bool visible = false;
    size_t next = 0;
    for (uint16_t frame = 0; frame < 300; frame++) {
        if (next < ARRAY_SIZE(transitions) && frame == transitions[next]) {
            visible = !visible;
            next++;
        }
        CHECK(TramDisplay_BlinkVisible(&display) == visible);
        Tram_Scanout(
            &bank, &font, display.first_cell,
            TramDisplay_BlinkVisible(&display), pixels, 640);
        CHECK_EQ_INT(pixels[0], visible ? 8 : 0);
        CHECK_EQ_INT(pixels[15], visible ? 8 : 0);
        CHECK_EQ_INT(pixels[16], 8);
        CHECK_EQ_INT(pixels[31], 8);
        TramDisplay_VSync(&display);
    }
    CHECK_EQ_INT(next, ARRAY_SIZE(transitions));
    CHECK_EQ_INT(display.first_cell, 8);
    CHECK(display.visible);
    CHECK(memcmp(&m_Tram, &saved, sizeof(m_Tram)) == 0);
    Mem_Free(pixels);
}

TEST(hidden_text_keeps_blinking_and_display_initialization_accepts_phase_seeds)
{
    TRAM_DISPLAY display;
    TramDisplay_Init(&display, 255);
    CHECK_EQ_INT(display.blink_frame, 127);
    CHECK_EQ_INT(display.first_cell, 0);
    CHECK(display.visible);
    CHECK(TramDisplay_BlinkVisible(&display));
    display.visible = false;
    display.first_cell = 65535;
    TramDisplay_VSync(&display);
    CHECK(!TramDisplay_BlinkVisible(&display));
    for (size_t edge = 1; edge < 32; edge++) {
        TramDisplay_VSync(&display);
        CHECK(!TramDisplay_BlinkVisible(&display));
    }
    TramDisplay_VSync(&display);
    CHECK(TramDisplay_BlinkVisible(&display));
    CHECK(!display.visible);
    CHECK_EQ_INT(display.first_cell, 65535);
}

TEST(visible_fill_keeps_both_plane_tails)
{
    Tram_Init();
    for (size_t cell = 0; cell < TRAM_PLANE_CELLS; cell++) {
        CHECK_EQ_INT(m_Tram.codes[cell], 0);
        CHECK_EQ_INT(m_Tram.attributes[cell], 0);
    }
    memset(&m_Tram, 0xa5, sizeof(m_Tram));
    Tram_Fill(0x20, 0x1234);
    for (size_t cell = 0; cell < TRAM_PLANE_CELLS; cell++) {
        CHECK_EQ_INT(m_Tram.codes[cell], cell < 2000 ? 0x20 : 0xa5a5);
        CHECK_EQ_INT(m_Tram.attributes[cell], cell < 2000 ? 0x1234 : 0xa5a5);
    }
}

TEST(gaiji_codes_keep_both_halves_and_spill_across_rows)
{
    Tram_Init();
    Tram_PutGaiji(79, 0, 0x127f, 0x34e1);
    CHECK_EQ_INT(m_Tram.codes[79], 0x7f56);
    CHECK_EQ_INT(m_Tram.codes[80], 0xff56);
    CHECK_EQ_INT(m_Tram.attributes[79], 0x34e1);
    CHECK_EQ_INT(m_Tram.attributes[80], 0x34e1);
    Tram_PutGaiji(79, 24, 128, 0x21);
    CHECK_EQ_INT(m_Tram.codes[1999], 0x57);
    CHECK_EQ_INT(m_Tram.codes[2000], 0x8057);
    CHECK_EQ_INT(m_Tram.attributes[2000], 0x21);
    Tram_PutGaiji(0, 0, 0, 0xe1);
    CHECK_EQ_INT(m_Tram.codes[0], 0x56);
    CHECK_EQ_INT(m_Tram.codes[1], 0x8056);
    Tram_PutGaiji(2, 0, 255, 0xe1);
    CHECK_EQ_INT(m_Tram.codes[2], 0x7f57);
    CHECK_EQ_INT(m_Tram.codes[3], 0xff57);
}

TEST(gaiji_strings_keep_overlap_and_offset_wrap)
{
    Tram_Init();
    const uint8_t glyphs[] = { 1, 128, 255, 0, 2 };
    Tram_PutGaijiString(78, 0, glyphs, 0xabcd);
    const uint16_t expected[] = {
        0x0156, 0x8156, 0x0057, 0x8057, 0x7f57, 0xff57,
    };
    for (size_t cell = 0; cell < 6; cell++) {
        CHECK_EQ_INT(m_Tram.codes[78 + cell], expected[cell]);
        CHECK_EQ_INT(m_Tram.attributes[78 + cell], 0xabcd);
    }
    CHECK_EQ_INT(m_Tram.codes[84], 0);
    const uint8_t empty[] = { 0 };
    const TRAM before = m_Tram;
    Tram_PutGaijiString(0, 0, empty, 0);
    CHECK(memcmp(&m_Tram, &before, sizeof(m_Tram)) == 0);
    Tram_PutGaijiString(32767, 0, glyphs, 0x1234);
    CHECK_EQ_INT(m_Tram.codes[0], 0x8156);
    CHECK_EQ_INT(m_Tram.codes[1], 0x0057);
    CHECK_EQ_INT(m_Tram.codes[2], 0x8057);
    CHECK_EQ_INT(m_Tram.codes[3], 0x7f57);
    CHECK_EQ_INT(m_Tram.codes[4], 0xff57);
    CHECK_EQ_INT(m_Tram.attributes[0], 0x1234);
    CHECK_EQ_INT(m_Tram.attributes[4], 0x1234);
    Tram_PutGaiji(4096, 0, 2, 0x4321);
    CHECK_EQ_INT(m_Tram.attributes[0], 0x0256);
    CHECK_EQ_INT(m_Tram.attributes[1], 0x8256);
}

TEST(attribute_boxes_preserve_codes_and_other_cells)
{
    memset(&m_Tram, 0xa5, sizeof(m_Tram));
    Tram_FillAttributes(4, 1, 51, 23, 0x0501);
    Tram_FillAttributes(79, 24, 79, 24, 0x8021);
    for (size_t cell = 0; cell < TRAM_PLANE_CELLS; cell++) {
        CHECK_EQ_INT(m_Tram.codes[cell], 0xa5a5);
        const size_t x = cell % 80;
        const size_t y = cell / 80;
        const uint16_t expected = cell == 1999       ? 0x8021
            : x >= 4 && x <= 51 && y >= 1 && y <= 23 ? 0x0501
                                                     : 0xa5a5;
        CHECK_EQ_INT(m_Tram.attributes[cell], expected);
    }
}

static uint16_t M_FontRow(
    const void *const context, const uint16_t code, const bool wide,
    const uint8_t y)
{
    if (wide) {
        return code == 0x2321 ? 0x4004 : 0;
    }
    return code == 'A' || code == 0x56 ? 0x2000 : 0;
}

TEST(scanout_alternates_custom_halves_and_pairs_normal_wide_glyphs)
{
    Tram_Init();
    GAIJI bank = {};
    for (size_t y = 0; y < 16; y++) {
        bank.rows[1][y] = 0x8001;
        bank.rows[130][y] = 0x4002;
    }
    const TRAM_FONT font = { M_FontRow, nullptr };
    const uint16_t codes[] = {
        0x0156, 0x8257, 0x8156, 'A', 0x0156, 0x2321, 0x0156, 0x8156,
    };
    for (size_t cell = 0; cell < 8; cell++) {
        m_Tram.codes[cell] = codes[cell];
        m_Tram.attributes[cell] = cell == 6 ? 0x81 : 0xe1;
    }
    uint8_t *const pixels = Mem_Alloc(640 * 400);
    Tram_Scanout(&bank, &font, 0, true, pixels, 640);
    for (size_t y = 0; y < 16; y++) {
        const uint8_t *const line = pixels + y * 640;
        CHECK_EQ_INT(line[0], 8);
        CHECK_EQ_INT(line[14], 8);
        CHECK_EQ_INT(line[16], 8);
        CHECK_EQ_INT(line[23], 0);
        CHECK_EQ_INT(line[26], 8);
        CHECK_EQ_INT(line[32], 8);
        CHECK_EQ_INT(line[41], 8);
        CHECK_EQ_INT(line[53], 5);
        CHECK_EQ_INT(line[48], 0);
        CHECK_EQ_INT(line[56], 8);
    }
    Mem_Free(pixels);
}

TEST(scanout_preserves_hidden_reverse_blink_and_cross_cell_lines)
{
    Tram_Init();
    GAIJI bank = {};
    const TRAM_FONT font = { M_FontRow, nullptr };
    m_Tram.attributes[0] = 0xe5;
    m_Tram.codes[1] = 'A';
    m_Tram.attributes[1] = 0x63;
    m_Tram.attributes[2] = 0x29;
    m_Tram.attributes[3] = 0x81;
    m_Tram.attributes[4] = 0x11;
    m_Tram.attributes[5] = 0x44;
    uint8_t *const pixels = Mem_Alloc(640 * 400);
    Tram_Scanout(&bank, &font, 0, false, pixels, 640);
    for (size_t y = 0; y < 16; y++) {
        const uint8_t *const line = pixels + y * 640;
        for (size_t x = 0; x < 8; x++) {
            CHECK_EQ_INT(line[x], 8);
            CHECK_EQ_INT(line[8 + x], 0);
            CHECK_EQ_INT(line[40 + x], 3);
        }
        CHECK_EQ_INT(line[36], 1);
    }
    for (size_t x = 16; x < 32; x++) {
        CHECK_EQ_INT(
            pixels[15 * 640 + x],
            x >= 20 && x < 24       ? 2
                : x >= 24 && x < 28 ? 5
                                    : 0);
    }
    Tram_Scanout(&bank, &font, 0, true, pixels, 640);
    CHECK_EQ_INT(pixels[10], 4);
    CHECK_EQ_INT(pixels[9], 0);
    const TRAM_FONT empty_font = {};
    Tram_Scanout(&bank, &empty_font, 0, true, pixels, 640);
    CHECK_EQ_INT(pixels[10], 0);
    CHECK_EQ_INT(pixels[0], 8);
    Mem_Free(pixels);
}

TEST(scanout_wraps_text_fetches_and_restarts_halves_at_each_row)
{
    Tram_Init();
    GAIJI bank = {};
    for (size_t y = 0; y < 16; y++) {
        bank.rows[1][y] = 0x8001;
    }
    m_Tram.codes[4095] = 0x0156;
    m_Tram.codes[0] = 0x8156;
    m_Tram.codes[79] = 0x8156;
    m_Tram.attributes[4095] = 0xe1;
    m_Tram.attributes[0] = 0xe1;
    m_Tram.attributes[79] = 0xe1;
    const TRAM before = m_Tram;
    const GAIJI bank_before = bank;
    const TRAM_FONT font = {};
    uint8_t *const pixels = Mem_Alloc(648 * 400);
    memset(pixels, 0xa5, 648 * 400);
    Tram_Scanout(&bank, &font, 65535, true, pixels, 648);
    CHECK_EQ_INT(pixels[0], 8);
    CHECK_EQ_INT(pixels[15], 8);
    CHECK_EQ_INT(pixels[16 * 648], 8);
    CHECK_EQ_INT(pixels[16 * 648 + 7], 0);
    for (size_t y = 0; y < 400; y++) {
        for (size_t x = 640; x < 648; x++) {
            CHECK_EQ_INT(pixels[y * 648 + x], 0xa5);
        }
    }
    CHECK(memcmp(&m_Tram, &before, sizeof(m_Tram)) == 0);
    CHECK(memcmp(&bank, &bank_before, sizeof(bank)) == 0);
    Mem_Free(pixels);
}

TEST(scanout_treats_zero_high_byte_as_ank_before_custom_half_selection)
{
    Tram_Init();
    GAIJI bank = {};
    bank.rows[0][0] = 0x8001;
    const TRAM_FONT font = { M_FontRow, nullptr };
    Tram_PutGaiji(0, 0, 0, 0xe1);
    uint8_t *const pixels = Mem_Alloc(640 * 400);
    Tram_Scanout(&bank, &font, 0, true, pixels, 640);
    CHECK_EQ_INT(pixels[2], 8);
    CHECK_EQ_INT(pixels[0], 0);
    CHECK_EQ_INT(pixels[8], 8);
    CHECK_EQ_INT(pixels[15], 0);
    Mem_Free(pixels);
}

TEST(blink_follows_elapsed_display_time_between_game_vsyncs)
{
    TRAM_DISPLAY display;
    TramDisplay_Init(&display, 0);
    display.visible = false;
    display.first_cell = 42;
    TramDisplay_Clock(&display, 540000);
    CHECK(!TramDisplay_BlinkVisible(&display));
    TramDisplay_Clock(&display, 550000);
    CHECK(TramDisplay_BlinkVisible(&display));
    TramDisplay_Clock(&display, 2250000);
    CHECK(TramDisplay_BlinkVisible(&display));
    TramDisplay_Clock(&display, 2270000);
    CHECK(!TramDisplay_BlinkVisible(&display));
    CHECK(!display.visible);
    CHECK_EQ_INT(display.first_cell, 42);
}
