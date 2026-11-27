#include "tests/test.h"
#include "th02/patterns.h"

#include "core/base/memory.h"

#include <string.h>

typedef struct {
    size_t allocations;
    size_t frees;
    size_t live_bytes;
} ALLOCATION_COUNT;

static void *M_Alloc(void *const context, const size_t size)
{
    ALLOCATION_COUNT *const count = context;
    count->allocations++;
    count->live_bytes += size;
    return Mem_Alloc(size);
}

static void M_Free(void *const context, void *const memory, const size_t size)
{
    ALLOCATION_COUNT *const count = context;
    count->frees++;
    count->live_bytes -= size;
    Mem_Free(memory);
}

static ALLOCATOR M_Allocator(ALLOCATION_COUNT *const count)
{
    return (ALLOCATOR) { .alloc = M_Alloc, .free = M_Free, .context = count };
}

TEST(planes_and_transparency_follow_every_color_key)
{
    PATTERN_BANK bank;
    PatternBank_Init(&bank, &g_HeapAllocator);
    uint8_t pixels[16];
    for (size_t i = 0; i < sizeof(pixels); i++) {
        pixels[i] = i;
    }
    for (uint8_t key = 0; key < 16; key++) {
        CHECK(Result_Ignore(PatternBank_Put(&bank, 0, 8, 2, pixels, key)));
        CHECK_EQ_INT(bank.next, 1);
        CHECK_EQ_INT(bank.entries[0].size, 0x0102);
        const uint8_t *const data = bank.entries[0].data;
        for (size_t pixel = 0; pixel < sizeof(pixels); pixel++) {
            const uint8_t bit = 0x80u >> (pixel % 8);
            CHECK_EQ_INT((data[pixel / 8] & bit) != 0, pixel != key);
            for (size_t plane = 0; plane < 4; plane++) {
                const bool set = (data[(plane + 1) * 2 + pixel / 8] & bit) != 0;
                CHECK_EQ_INT(set, pixel != key && (pixel & (1u << plane)) != 0);
            }
        }
    }
    PatternBank_Reset(&bank);
}

TEST(replacement_owns_pixels_and_releases_the_old_allocation)
{
    ALLOCATION_COUNT count = {};
    const ALLOCATOR allocator = M_Allocator(&count);
    PATTERN_BANK bank;
    PatternBank_Init(&bank, &allocator);
    uint8_t pixels[16];
    memset(pixels, 15, sizeof(pixels));
    CHECK(Result_Ignore(PatternBank_Put(&bank, 3, 16, 1, pixels, 0)));
    memset(pixels, 0, sizeof(pixels));
    CHECK_EQ_INT(bank.entries[3].data[0], 255);
    CHECK_EQ_INT(bank.entries[3].data[9], 255);
    CHECK(Result_Ignore(PatternBank_Put(&bank, 3, 8, 1, pixels, 0)));
    CHECK_EQ_INT(bank.next, 4);
    CHECK_EQ_INT(count.allocations, 2);
    CHECK_EQ_INT(count.frees, 1);
    CHECK_EQ_INT(count.live_bytes, 5);
    PatternBank_Reset(&bank);
    CHECK_EQ_INT(count.live_bytes, 0);
    CHECK_EQ_INT(count.frees, 2);
    PatternBank_Reset(&bank);
    CHECK_EQ_INT(count.frees, 2);
}

TEST(cancel_last_walks_back_over_holes_and_rejects_missing_slots)
{
    PATTERN_BANK bank;
    PatternBank_Init(&bank, &g_HeapAllocator);
    const uint8_t pixels[8] = {};
    CHECK(Result_Ignore(PatternBank_Put(&bank, 3, 8, 1, pixels, 0)));
    CHECK(Result_Ignore(PatternBank_Put(&bank, 8, 8, 1, pixels, 0)));
    CHECK(!Result_Ignore(PatternBank_Cancel(&bank, 2)));
    CHECK(!Result_Ignore(PatternBank_Cancel(&bank, 9)));
    CHECK(Result_Ignore(PatternBank_Cancel(&bank, 8)));
    CHECK_EQ_INT(bank.next, 4);
    CHECK(bank.entries[8].data == nullptr);
    CHECK_EQ_INT(bank.entries[8].size, 0);
    CHECK(Result_Ignore(PatternBank_Cancel(&bank, 3)));
    CHECK_EQ_INT(bank.next, 0);
    PatternBank_Reset(&bank);
}

TEST(moving_append_cursor_keeps_later_slots_and_reset_frees_them)
{
    ALLOCATION_COUNT count = {};
    const ALLOCATOR allocator = M_Allocator(&count);
    PATTERN_BANK bank;
    PatternBank_Init(&bank, &allocator);
    const uint8_t pixels[8] = { 1 };
    CHECK(Result_Ignore(PatternBank_Put(&bank, 140, 8, 1, pixels, 0)));
    CHECK(Result_Ignore(PatternBank_SetNext(&bank, 128)));
    CHECK_EQ_INT(bank.entries[140].size, 0x0101);
    CHECK(!Result_Ignore(PatternBank_Cancel(&bank, 140)));
    CHECK(Result_Ignore(PatternBank_Put(&bank, 128, 8, 1, pixels, 0)));
    CHECK_EQ_INT(bank.next, 129);
    CHECK_EQ_INT(bank.entries[140].data[0], 128);
    CHECK(!Result_Ignore(PatternBank_SetNext(&bank, 513)));
    CHECK_EQ_INT(bank.next, 129);
    PatternBank_Reset(&bank);
    CHECK_EQ_INT(count.live_bytes, 0);
    CHECK_EQ_INT(count.frees, 2);
    CHECK_EQ_INT(bank.next, 0);
}

TEST(clean_uses_inclusive_bounds_and_preserves_other_patterns)
{
    PATTERN_BANK bank;
    PatternBank_Init(&bank, &g_HeapAllocator);
    const uint8_t pixels[8] = {};
    for (uint16_t slot = 0; slot < 6; slot += 2) {
        CHECK(Result_Ignore(PatternBank_Put(&bank, slot, 8, 1, pixels, 0)));
    }
    PatternBank_Clean(&bank, 4, 2);
    CHECK_EQ_INT(bank.next, 5);
    PatternBank_Clean(&bank, 1, 2);
    CHECK(bank.entries[0].data != nullptr);
    CHECK(bank.entries[2].data == nullptr);
    CHECK(bank.entries[4].data != nullptr);
    PatternBank_Clean(&bank, 3, UINT16_MAX);
    CHECK_EQ_INT(bank.next, 1);
    PatternBank_Clean(&bank, 0, UINT16_MAX);
    CHECK_EQ_INT(bank.next, 0);
    PatternBank_Reset(&bank);
}

TEST(bfnt_appends_at_cursor_and_keeps_prefix_on_capacity_failure)
{
    PATTERN_BANK bank;
    PatternBank_Init(&bank, &g_HeapAllocator);
    const uint8_t pixels[24] = { 1, 2, 3, 4, 5, 6, 7, 8, 15 };
    BFNT sprites = { .width = 8,
                     .height = 1,
                     .count = 3,
                     .first = 40,
                     .bits_per_pixel = 4,
                     .pixels = pixels };
    CHECK(Result_Ignore(PatternBank_SetNext(&bank, 510)));
    CHECK(!Result_Ignore(PatternBank_AppendBfnt(&bank, &sprites)));
    CHECK_EQ_INT(bank.next, 512);
    CHECK_EQ_INT(bank.entries[510].size, 0x0101);
    CHECK_EQ_INT(bank.entries[510].data[0], 255);
    CHECK_EQ_INT(bank.entries[511].data[0], 128);
    CHECK(bank.entries[40].data == nullptr);
    sprites.bits_per_pixel = 1;
    CHECK(!Result_Ignore(PatternBank_AppendBfnt(&bank, &sprites)));
    CHECK_EQ_INT(bank.next, 512);
    PatternBank_Reset(&bank);
}

TEST(invalid_registration_preserves_bank_and_allocates_nothing)
{
    ALLOCATION_COUNT count = {};
    const ALLOCATOR allocator = M_Allocator(&count);
    PATTERN_BANK bank;
    PatternBank_Init(&bank, &allocator);
    uint8_t pixels[8] = {};
    CHECK(Result_Ignore(PatternBank_Put(&bank, 2, 8, 1, pixels, 0)));
    CHECK(!Result_Ignore(PatternBank_Put(&bank, 512, 8, 1, pixels, 0)));
    CHECK(!Result_Ignore(PatternBank_Put(&bank, 2, 0, 1, pixels, 0)));
    CHECK(!Result_Ignore(PatternBank_Put(&bank, 2, 7, 1, pixels, 0)));
    CHECK(!Result_Ignore(PatternBank_Put(&bank, 2, 2048, 1, pixels, 0)));
    CHECK(!Result_Ignore(PatternBank_Put(&bank, 2, 8, 0, pixels, 0)));
    CHECK(!Result_Ignore(PatternBank_Put(&bank, 2, 8, 256, pixels, 0)));
    CHECK(!Result_Ignore(PatternBank_Put(&bank, 2, 2040, 255, pixels, 0)));
    CHECK(!Result_Ignore(PatternBank_Put(&bank, 2, 8, 1, pixels, 16)));
    CHECK(!Result_Ignore(PatternBank_Put(&bank, 2, 8, 1, nullptr, 0)));
    pixels[7] = 16;
    CHECK(!Result_Ignore(PatternBank_Put(&bank, 2, 8, 1, pixels, 0)));
    CHECK_EQ_INT(count.allocations, 1);
    CHECK_EQ_INT(count.frees, 0);
    CHECK_EQ_INT(bank.next, 3);
    CHECK_EQ_INT(bank.entries[2].size, 0x0101);
    PatternBank_Reset(&bank);
    CHECK_EQ_INT(count.live_bytes, 0);
}
