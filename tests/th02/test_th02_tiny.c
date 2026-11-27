#include "th02/patterns.h"

#include "test.h"

TEST(tiny_records_sort_colors_and_grow_storage_when_needed)
{
    PATTERN_BANK bank;
    PatternBank_Init(&bank, &g_HeapAllocator);
    const uint8_t pixels[48] = { 8, 2, 15, 1, 0, 8, 2, 15, 1, 0 };
    CHECK(Result_Ignore(PatternBank_Put(&bank, 3, 16, 3, pixels, 0)));
    CHECK(Result_Ignore(PatternBank_ConvertTiny(&bank, 3)));
    const uint8_t expected[] = {
        0x80, 1,  0x10, 0x80, 0,    0, 0,    0, 0x80, 2, 0x42, 0,
        0,    0,  0,    0,    0x80, 8, 0x84, 0, 0,    0, 0,    0,
        0x80, 15, 0x21, 0,    0,    0, 0,    0, 0,    0,
    };
    CHECK_EQ_INT(bank.next, 4);
    CHECK_EQ_INT(bank.entries[3].size, 0x0203);
    CHECK_EQ_INT(bank.entries[3].storage_size, sizeof(expected));
    CHECK(memcmp(bank.entries[3].data, expected, sizeof(expected)) == 0);
    PatternBank_Reset(&bank);
}

TEST(tiny_conversion_preserves_the_tail_and_supports_covered_zero)
{
    PATTERN_BANK bank;
    PatternBank_Init(&bank, &g_HeapAllocator);
    uint8_t pixels[32];
    memset(pixels, 15, sizeof(pixels));
    pixels[0] = pixels[9] = 0;
    pixels[2] = 7;
    CHECK(Result_Ignore(PatternBank_Put(&bank, 0, 16, 2, pixels, 15)));
    uint8_t before[20];
    memcpy(before, bank.entries[0].data, sizeof(before));
    CHECK(Result_Ignore(PatternBank_ConvertTiny(&bank, 0)));
    const uint8_t expected[] = { 0x80, 0,    0x80, 0x40, 0, 0, 0x80,
                                 7,    0x20, 0,    0,    0, 0, 0 };
    CHECK(memcmp(bank.entries[0].data, expected, sizeof(expected)) == 0);
    CHECK(
        memcmp(
            bank.entries[0].data + sizeof(expected), before + sizeof(expected),
            sizeof(before) - sizeof(expected))
        == 0);
    CHECK_EQ_INT(bank.entries[0].storage_size, sizeof(before));
    PatternBank_Reset(&bank);
}

TEST(empty_coverage_writes_only_the_terminator)
{
    PATTERN_BANK bank;
    PatternBank_Init(&bank, &g_HeapAllocator);
    const uint8_t pixels[16] = {};
    CHECK(Result_Ignore(PatternBank_Put(&bank, 0, 16, 1, pixels, 0)));
    CHECK(Result_Ignore(PatternBank_ConvertTiny(&bank, 0)));
    CHECK_EQ_INT(bank.entries[0].data[0], 0);
    CHECK_EQ_INT(bank.entries[0].data[1], 0);
    CHECK_EQ_INT(bank.entries[0].storage_size, 10);
    PatternBank_Reset(&bank);
}

TEST(fifth_color_failure_leaves_the_pattern_and_cursor_unchanged)
{
    PATTERN_BANK bank;
    PatternBank_Init(&bank, &g_HeapAllocator);
    const uint8_t pixels[32] = { 1, 2, 3, 4, 5 };
    CHECK(Result_Ignore(PatternBank_Put(&bank, 2, 16, 2, pixels, 0)));
    uint8_t before[20];
    memcpy(before, bank.entries[2].data, sizeof(before));
    const uint8_t *const address = bank.entries[2].data;
    CHECK(!Result_Ignore(PatternBank_ConvertTiny(&bank, 2)));
    CHECK(bank.entries[2].data == address);
    CHECK(memcmp(bank.entries[2].data, before, sizeof(before)) == 0);
    CHECK_EQ_INT(bank.next, 3);
    CHECK_EQ_INT(bank.entries[2].storage_size, sizeof(before));
    PatternBank_Reset(&bank);
}

TEST(rejects_absent_hidden_and_odd_sized_patterns)
{
    PATTERN_BANK bank;
    PatternBank_Init(&bank, &g_HeapAllocator);
    CHECK(!Result_Ignore(PatternBank_ConvertTiny(&bank, 0)));
    CHECK(!Result_Ignore(PatternBank_ConvertTiny(&bank, 512)));
    const uint8_t pixels[8] = { 1 };
    CHECK(Result_Ignore(PatternBank_Put(&bank, 4, 8, 1, pixels, 0)));
    CHECK(!Result_Ignore(PatternBank_ConvertTiny(&bank, 4)));
    CHECK_EQ_INT(bank.entries[4].data[0], 0x80);
    CHECK(Result_Ignore(PatternBank_SetNext(&bank, 4)));
    CHECK(!Result_Ignore(PatternBank_ConvertTiny(&bank, 4)));
    CHECK_EQ_INT(bank.next, 4);
    PatternBank_Reset(&bank);
}
