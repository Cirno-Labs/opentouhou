#include "tests/test.h"
#include "tests/th02/th02_palette_fixture.h"
#include "th02/palette.h"

#include "core/io/reader.h"

#include <string.h>

TEST(stock_colors_match_pc98_index_bits)
{
    Palette_Init();
    CHECK_EQ_INT(p_Palette->tone, 100);
    CHECK_EQ_INT(p_Palette->colors[1][2], 255);
    CHECK_EQ_INT(p_Palette->colors[2][0], 255);
    CHECK_EQ_INT(p_Palette->colors[4][1], 255);
    CHECK_EQ_INT(p_Palette->colors[8][0], 119);
    CHECK_EQ_INT(p_Palette->colors[8][1], 119);
    CHECK_EQ_INT(p_Palette->colors[8][2], 119);
    CHECK_EQ_INT(p_Palette->colors[15][0], 170);
    CHECK_EQ_INT(p_Palette->colors[15][1], 170);
    CHECK_EQ_INT(p_Palette->colors[15][2], 170);
    memset(&g_Video, 0, sizeof(g_Video));
    Palette_Show();
    CHECK_EQ_INT(g_Video.palette[1].blue, 255);
    CHECK_EQ_INT(g_Video.palette[8].red, 119);
    p_Palette->tone = 0;
    memset(p_Palette->colors, 0, sizeof(p_Palette->colors));
    Palette_Reset();
    CHECK_EQ_INT(p_Palette->tone, 100);
    CHECK_EQ_INT(p_Palette->colors[1][2], 255);
    CHECK_EQ_INT(g_Video.palette[1].blue, 255);
}

TEST(import_keeps_raw_channels_and_does_not_change_the_display)
{
    Palette_Init();
    p_Palette->tone = 50;
    GFX_COLOR colors[16];
    for (size_t i = 0; i < 16; i++) {
        colors[i] = (GFX_COLOR) { 0xab, 0x53, 0xff, 0 };
    }
    memset(&g_Video, 0, sizeof(g_Video));
    g_Video.palette[0].red = 123;
    Palette_Import(colors);
    CHECK_EQ_INT(p_Palette->colors[0][0], 0xab);
    CHECK_EQ_INT(p_Palette->colors[0][1], 0x53);
    CHECK_EQ_INT(p_Palette->colors[0][2], 0xff);
    CHECK_EQ_INT(p_Palette->tone, 50);
    CHECK_EQ_INT(g_Video.palette[0].red, 123);
    Palette_Show();
    CHECK_EQ_INT(g_Video.palette[0].red, 5 * 17);
    CHECK_EQ_INT(g_Video.palette[0].green, 2 * 17);
    CHECK_EQ_INT(g_Video.palette[0].blue, 7 * 17);
    CHECK_EQ_INT(g_Video.palette[0].alpha, 255);
    CHECK_EQ_INT(p_Palette->colors[0][0], 0xab);
}

TEST(tone_rounding_and_clamps_keep_logical_colors_and_raw_tone)
{
    Palette_Init();
    memset(p_Palette->colors, 0, sizeof(p_Palette->colors));
    p_Palette->colors[0][0] = 0xab;
    p_Palette->colors[0][1] = 0x53;
    p_Palette->colors[0][2] = 0xff;
    memset(&g_Video, 0, sizeof(g_Video));
    const int16_t tones[] = { INT16_MIN, -1,  0,   50,  99,
                              100,       101, 150, 200, INT16_MAX };
    const uint8_t expected[][3] = {
        { 0, 0, 0 },    { 0, 0, 0 },    { 0, 0, 0 },   { 5, 2, 7 },
        { 9, 4, 14 },   { 10, 5, 15 },  { 11, 6, 15 }, { 13, 10, 15 },
        { 15, 15, 15 }, { 15, 15, 15 },
    };
    for (size_t i = 0; i < sizeof(tones) / sizeof(tones[0]); i++) {
        Palette_SetTone(tones[i]);
        CHECK_EQ_INT(p_Palette->tone, tones[i]);
        CHECK_EQ_INT(g_Video.palette[0].red, expected[i][0] * 17);
        CHECK_EQ_INT(g_Video.palette[0].green, expected[i][1] * 17);
        CHECK_EQ_INT(g_Video.palette[0].blue, expected[i][2] * 17);
        CHECK_EQ_INT(g_Video.palette[0].alpha, 255);
        CHECK_EQ_INT(p_Palette->colors[0][0], 0xab);
        CHECK_EQ_INT(p_Palette->colors[0][1], 0x53);
        CHECK_EQ_INT(p_Palette->colors[0][2], 0xff);
    }
}

TEST(positive_fades_wait_for_alignment_and_the_full_requested_edge_count)
{
    const int16_t starts[] = { 0, 100, 200, 100 };
    const int16_t targets[] = { 100, 0, 100, 200 };
    const int16_t speeds[] = { 1, 2, 10, INT16_MAX };
    for (int kind = PALETTE_BLACK_IN; kind <= PALETTE_WHITE_OUT; kind++) {
        for (size_t speed = 0; speed < sizeof(speeds) / sizeof(speeds[0]);
             speed++) {
            Palette_Init();
            memset(&g_Video, 0, sizeof(g_Video));
            g_Video.palette[7].red = 123;
            PALETTE_FADE fade;
            Palette_BeginFade(&fade, kind, speeds[speed]);
            CHECK_EQ_INT(p_Palette->tone, starts[kind]);
            CHECK_EQ_INT(g_Video.palette[7].red, 123);
            const int final_edge = 1 + 17 * speeds[speed];
            for (int edge = 1; edge <= final_edge; edge++) {
                const bool complete = Palette_FadeVSync(&fade);
                const int step = (edge - 1) / speeds[speed];
                const int expected = step == 17 ? targets[kind]
                                                : starts[kind]
                        + (targets[kind] > starts[kind] ? 6 : -6) * step;
                CHECK_EQ_INT(p_Palette->tone, expected);
                CHECK_EQ_INT(complete, edge == final_edge);
                CHECK_EQ_INT(fade.active, edge != final_edge);
            }
            const VIDEO saved = g_Video;
            CHECK(Palette_FadeVSync(&fade));
            CHECK(memcmp(&g_Video, &saved, sizeof(g_Video)) == 0);
        }
    }
}

TEST(nonpositive_fades_finish_on_the_initial_edge_and_use_live_colors)
{
    const int16_t speeds[] = { 0, -1, INT16_MIN };
    for (size_t speed = 0; speed < sizeof(speeds) / sizeof(speeds[0]);
         speed++) {
        Palette_Init();
        memset(&g_Video, 0, sizeof(g_Video));
        g_Video.palette[7].red = 123;
        PALETTE_FADE fade;
        Palette_BeginFade(&fade, PALETTE_BLACK_IN, speeds[speed]);
        p_Palette->colors[7][0] = 0;
        p_Palette->colors[7][1] = 0;
        CHECK_EQ_INT(g_Video.palette[7].red, 123);
        CHECK(Palette_FadeVSync(&fade));
        CHECK_EQ_INT(p_Palette->tone, 100);
        CHECK_EQ_INT(g_Video.palette[7].red, 0);
        CHECK_EQ_INT(g_Video.palette[7].green, 0);
        CHECK_EQ_INT(g_Video.palette[7].blue, 255);
    }

    Palette_Init();
    memset(&g_Video, 0, sizeof(g_Video));
    PALETTE_FADE fade;
    Palette_BeginFade(&fade, PALETTE_BLACK_IN, 2);
    for (int edge = 1; edge <= 17; edge++) {
        p_Palette->colors[7][0] = 0;
        p_Palette->colors[7][1] = 0x53;
        p_Palette->colors[7][2] = 0xff;
        CHECK(!Palette_FadeVSync(&fade));
    }
    CHECK_EQ_INT(p_Palette->tone, 48);
    CHECK_EQ_INT(g_Video.palette[7].red, 0);
    CHECK_EQ_INT(g_Video.palette[7].green, 34);
    CHECK_EQ_INT(g_Video.palette[7].blue, 119);
}

TEST(rgb_reads_only_one_palette_and_keeps_tone)
{
    uint8_t bytes[51];
    for (size_t i = 0; i < sizeof(bytes); i++) {
        bytes[i] = i % 16;
    }
    READER reader;
    Reader_OpenBuffer(&reader, bytes, sizeof(bytes), nullptr);

    Palette_Init();
    p_Palette->tone = -123;
    CHECK(Result_Ignore(Palette_ReadRgb(&reader, p_Palette->colors)));
    CHECK_EQ_INT(Reader_Tell(&reader), 48);
    CHECK_EQ_INT(Reader_Remaining(&reader), 3);
    CHECK_EQ_INT(p_Palette->colors[0][0], 0);
    CHECK_EQ_INT(p_Palette->colors[0][1], 17);
    CHECK_EQ_INT(p_Palette->colors[0][2], 34);
    CHECK_EQ_INT(p_Palette->colors[15][0], 221);
    CHECK_EQ_INT(p_Palette->colors[15][1], 238);
    CHECK_EQ_INT(p_Palette->colors[15][2], 255);
    CHECK_EQ_INT(p_Palette->tone, -123);
    Reader_Close(&reader);
}

TEST(rgb_short_stream_expands_the_retained_channels_without_rejecting_them)
{
    const uint8_t bytes[] = { 0x22, 0x01, 0x04, 0x08 };

    Palette_Init();
    memset(p_Palette->colors, 0x33, sizeof(p_Palette->colors));
    p_Palette->colors[1][1] = 0x24;
    READER reader;
    Reader_OpenBuffer(&reader, bytes, sizeof(bytes), nullptr);
    CHECK(Result_Ignore(Palette_ReadRgb(&reader, p_Palette->colors)));
    CHECK_EQ_INT(Reader_Tell(&reader), 4);
    CHECK_EQ_INT(p_Palette->colors[0][0], 0x22);
    CHECK_EQ_INT(p_Palette->colors[0][1], 0x11);
    CHECK_EQ_INT(p_Palette->colors[0][2], 0x44);
    CHECK_EQ_INT(p_Palette->colors[1][0], 0x88);
    CHECK_EQ_INT(p_Palette->colors[1][1], 0x64);
    CHECK_EQ_INT(p_Palette->colors[15][2], 0x33);
    Reader_Close(&reader);
    p_Palette->colors[0][0] = 0x08;
    Reader_OpenBuffer(&reader, nullptr, 0, nullptr);
    CHECK(Result_Ignore(Palette_ReadRgb(&reader, p_Palette->colors)));
    CHECK_EQ_INT(p_Palette->colors[0][0], 0x88);
    CHECK(!Reader_Failed(&reader));
    Reader_Close(&reader);
}

TEST(rgb_prefailed_reader_keeps_the_palette_unchanged)
{
    READER reader;
    Reader_OpenBuffer(&reader, nullptr, 0, nullptr);
    Reader_ReadU8(&reader);

    Palette_Init();
    const PALETTE_STATE before = (*p_Palette);
    CHECK(!Result_Ignore(Palette_ReadRgb(&reader, p_Palette->colors)));
    CHECK(memcmp(&before, p_Palette, sizeof((*p_Palette))) == 0);
    Reader_Close(&reader);
}

TEST(resource_rgb_decode_keeps_live_palette_independent)
{
    Palette_Init();
    Palette_SetTone(134);
    const PALETTE_STATE before = (*p_Palette);
    uint8_t colors[16][3] = {};
    const uint8_t bytes[] = { 1, 2, 3 };
    READER reader;
    Reader_OpenBuffer(&reader, bytes, sizeof(bytes), nullptr);
    CHECK(Result_Ignore(Palette_ReadRgb(&reader, colors)));
    Reader_Close(&reader);
    CHECK_EQ_INT(colors[0][0], 17);
    CHECK_EQ_INT(colors[0][1], 34);
    CHECK_EQ_INT(colors[0][2], 51);
    CHECK(memcmp(p_Palette, &before, sizeof(before)) == 0);
}
