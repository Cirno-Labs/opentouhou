#include "core/base/cp932.h"
#include "core/base/memory.h"
#include "core/base/utils.h"

#include "test.h"

#include <stdint.h>

typedef struct {
    const char *utf8;
    const char *cp932;
} SAMPLE;

static const SAMPLE m_Samples[] = {
    { "東方紅魔郷", "\x93\x8C\x95\xFB\x8D\x67\x96\x82\x8B\xBD" },
    { "ＭＳ ゴシック", "\x82\x6C\x82\x72\x20\x83\x53\x83\x56\x83\x62\x83\x4E" },
    { "ｱｲｳｴｵ ﾃｽﾄ", "\xB1\xB2\xB3\xB4\xB5\x20\xC3\xBD\xC4" },
    { "博麗 霊夢", "\x94\x8E\x97\xED\x20\x97\xEC\x96\xB2" },
    { "魔理沙「よう」",
      "\x96\x82\x97\x9D\x8D\xB9\x81\x75\x82\xE6\x82\xA4\x81\x76" },
    { "①Ⅰ㈱", "\x87\x40\x87\x54\x87\x8A" },
    { "ⅰ纊", "\xFA\x40\xFA\x5C" },
    { "～－", "\x81\x60\x81\x7C" },
    { "C:\\th06~ score.dat", "C:\\th06~ score.dat" },
    { "", "" },
};

static char *M_ToUtf8(const char *const cp932, const size_t size)
{
    char *output;
    if (!Result_Log(Cp932_Decode(cp932, size, &output), LOG_LEVEL_ERROR)) {
        return nullptr;
    }
    return output;
}

static char *M_ToCp932(const char *const utf8)
{
    char *output;
    if (!Result_Log(
            Cp932_Encode(utf8, strlen(utf8), &output), LOG_LEVEL_ERROR)) {
        return nullptr;
    }
    return output;
}

static bool M_Cp932Fails(const char *const cp932, const size_t size)
{
    char *output = (char *)1;
    const bool failed = !Result_Ignore(Cp932_Decode(cp932, size, &output));
    return failed && output == nullptr;
}

static bool M_Utf8Fails(const char *const utf8, const size_t size)
{
    char *output = (char *)1;
    const bool failed = !Result_Ignore(Cp932_Encode(utf8, size, &output));
    return failed && output == nullptr;
}

TEST(samples_convert_both_ways)
{
    for (size_t i = 0; i < ARRAY_SIZE(m_Samples); i++) {
        const SAMPLE *const sample = &m_Samples[i];
        char *const utf8 = M_ToUtf8(sample->cp932, strlen(sample->cp932));
        CHECK_EQ_STR(utf8, sample->utf8);
        char *const cp932 = M_ToCp932(sample->utf8);
        CHECK_EQ_STR(cp932, sample->cp932);
        Mem_Free(utf8);
        Mem_Free(cp932);
    }
}

TEST(duplicate_forms_decode_alike_and_encode_as_windows_does)
{
    char *const nec_selected = M_ToUtf8("\xEE\xEF\xED\x40", 4);
    CHECK_EQ_STR(nec_selected, "ⅰ纊");
    char *const nec_row_13 = M_ToUtf8("\x87\x90", 2);
    CHECK_EQ_STR(nec_row_13, "≒");
    char *const encoded = M_ToCp932("≒");
    CHECK_EQ_STR(encoded, "\x81\xE0");
    Mem_Free(nec_selected);
    Mem_Free(nec_row_13);
    Mem_Free(encoded);
}

TEST(only_the_given_size_is_converted)
{
    char *const utf8 = M_ToUtf8("\x93\x8C\x95\xFB\x8D\x67", 4);
    CHECK_EQ_STR(utf8, "東方");
    Mem_Free(utf8);
}

TEST(every_double_byte_character_round_trips)
{
    int32_t characters = 0;
    int32_t failures = 0;
    for (int lead = 0x81; lead <= 0xFC; lead++) {
        if (lead >= 0xA0 && lead <= 0xDF) {
            continue;
        }
        for (int trail = 0x40; trail <= 0xFC; trail++) {
            const char bytes[] = { (char)lead, (char)trail };
            char *utf8;
            if (!Result_Ignore(Cp932_Decode(bytes, 2, &utf8))) {
                continue;
            }
            characters++;
            char *cp932;
            char *again = nullptr;
            if (!Result_Ignore(Cp932_Encode(utf8, strlen(utf8), &cp932))) {
                failures++;
            } else if (
                !Result_Ignore(Cp932_Decode(cp932, strlen(cp932), &again))
                || strcmp(again, utf8) != 0) {
                failures++;
            }
            Mem_Free(utf8);
            Mem_Free(cp932);
            Mem_Free(again);
        }
    }
    CHECK_EQ_INT(failures, 0);
    CHECK_EQ_INT(characters, 9604);
}

TEST(invalid_cp932_fails)
{
    CHECK(M_Cp932Fails("\x93", 1));
    CHECK(M_Cp932Fails("abc\x93", 4));
    CHECK(M_Cp932Fails("\x81\x7F", 2));
    CHECK(M_Cp932Fails("\x81\x20", 2));
    CHECK(M_Cp932Fails("\x85\x40", 2));
    CHECK(M_Cp932Fails("ab\0cd", 5));
}

TEST(invalid_cp932_names_the_offset)
{
    char *output;
    const RESULT result = Cp932_Decode("\x82\xA0\x85\x40", 4, &output);
    CHECK_EQ_STR(result.message, "Invalid CP932 byte 0x85 at offset 2");
    CHECK(!Result_Ignore(result));
}

TEST(invalid_utf8_fails)
{
    CHECK(M_Utf8Fails("\x80", 1));
    CHECK(M_Utf8Fails("\xC0\xAF", 2));
    CHECK(M_Utf8Fails("\xE0\x80\xAF", 3));
    CHECK(M_Utf8Fails("\xED\xA0\x80", 3));
    CHECK(M_Utf8Fails("\xE6\x9D", 2));
    CHECK(M_Utf8Fails("\xE6\x9D\x41", 3));
    CHECK(M_Utf8Fails("\xF4\x90\x80\x80", 4));
    CHECK(M_Utf8Fails("\xFF", 1));
    CHECK(M_Utf8Fails("ab\0cd", 5));
}

TEST(characters_without_a_cp932_form_fail)
{
    char *output;
    const RESULT result = Cp932_Encode("東方é", 8, &output);
    CHECK_EQ_STR(result.message, "U+00E9 at offset 6 has no CP932 form");
    CHECK(!Result_Ignore(result));
    CHECK(M_Utf8Fails("😀", 4));
}
