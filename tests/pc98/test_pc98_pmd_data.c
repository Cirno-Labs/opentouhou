#include "tests/test.h"
#include "tests/test_allocator.h"
#include "tests/test_dir.h"

#include "core/base/memory.h"
#include "core/io/file.h"
#include "core/io/reader.h"
#include "pc98/pmd/data.h"
#include "pc98/pmd/effect.h"

#include <string.h>

#define M_BANK_SIZE (259 + 3 * 26 + 2)

static void M_Word(uint8_t *const bytes, const uint16_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
}

static void M_Bank(uint8_t bytes[M_BANK_SIZE])
{
    memset(bytes, 0, M_BANK_SIZE);
    for (size_t i = 0; i < PMD_EFFECT_COUNT; i++) {
        M_Word(bytes + i * 2, (uint16_t)(256 + i % 3));
    }
    M_Word(bytes + 254, 259);
    memset(bytes + 256, 0x80, 3);
    for (size_t i = 0; i < 3; i++) {
        bytes[259 + i * 26] = i == 1 ? 255 : 3;
        for (size_t j = 0; j < PMD_INSTRUMENT_SIZE; j++) {
            bytes[260 + i * 26 + j] = (uint8_t)(i * 31 + j);
        }
    }
    bytes[M_BANK_SIZE - 1] = 0xff;
}

static RESULT M_Load(
    const uint8_t *const bytes, const size_t size,
    TEST_ALLOCATOR *const allocator, PMD_EFFECTS **const bank)
{
    READER reader;
    Reader_OpenBuffer(&reader, bytes, size, nullptr);
    return PmdData_LoadEffects(&reader, &allocator->allocator, bank);
}

static void M_Release(TEST_ALLOCATOR *const allocator, PMD_EFFECTS *const bank)
{
    Allocator_Free(&allocator->allocator, bank, sizeof(*bank) + bank->size);
    CHECK_EQ_INT(allocator->live_blocks, 0);
    CHECK_EQ_INT(allocator->live_bytes, 0);
    CHECK_EQ_INT(allocator->size_errors, 0);
}

static void M_Reject(const uint8_t *const bytes, const size_t size)
{
    TEST_ALLOCATOR allocator;
    TestAllocator_Init(&allocator);
    allocator.allocations_left = 0;
    PMD_EFFECTS *bank;
    PMD_EFFECTS previous;
    bank = &previous;
    CHECK(!Result_Ignore(M_Load(bytes, size, &allocator, &bank)));
    CHECK(bank == nullptr);
    CHECK_EQ_INT(allocator.live_blocks, 0);
}

TEST(owns_bytes_and_resolves_shared_streams_and_first_instrument)
{
    uint8_t bytes[M_BANK_SIZE];
    M_Bank(bytes);
    TEST_ALLOCATOR allocator;
    TestAllocator_Init(&allocator);
    PMD_EFFECTS *bank;
    CHECK(Result_Log(
        M_Load(bytes, sizeof(bytes), &allocator, &bank), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(allocator.live_blocks, 1);
    CHECK_EQ_INT(allocator.live_bytes, sizeof(*bank) + sizeof(bytes));
    CHECK(bank->bytes != bytes);
    CHECK(memcmp(bank->bytes, bytes, sizeof(bytes)) == 0);
    memset(bytes, 0x77, sizeof(bytes));
    CHECK_EQ_INT(bank->instrument_count, 3);
    PMD_EFFECT_PLAYER player;
    CHECK(Result_Ignore(
        PmdEffect_Init(&player, bank, &allocator.allocator, nullptr, 2)));
    for (uint8_t i = 0; i < PMD_EFFECT_COUNT; i++) {
        CHECK(Result_Ignore(PmdEffect_Play(&player, i)));
        CHECK(Result_Ignore(PmdEffect_Step(&player, 0, false)));
        CHECK(!player.playing);
    }
    Allocator_Free(&allocator.allocator, player.work, player.work_size);
    CHECK(PmdData_Instrument(bank, 3) == bank->bytes + 260);
    CHECK(PmdData_Instrument(bank, 255) == bank->bytes + 286);
    CHECK(PmdData_Instrument(bank, 0) == nullptr);
    M_Release(&allocator, bank);
}

TEST(rejects_bad_extents_offsets_and_instrument_trailers_without_allocation)
{
    uint8_t bytes[M_BANK_SIZE];
    M_Bank(bytes);
    for (size_t size = 0; size < sizeof(bytes); size++) {
        M_Reject(bytes, size);
    }
    for (size_t i = 0; i < 128; i++) {
        const uint16_t invalid[] = { 0, 255, 65535 };
        for (size_t j = 0; j < ARRAY_SIZE(invalid); j++) {
            M_Bank(bytes);
            M_Word(bytes + i * 2, invalid[j]);
            M_Reject(bytes, sizeof(bytes));
        }
    }
    M_Bank(bytes);
    M_Word(bytes, 259);
    M_Reject(bytes, sizeof(bytes));
    M_Bank(bytes);
    bytes[sizeof(bytes) - 2] = 1;
    M_Reject(bytes, sizeof(bytes));
    M_Bank(bytes);
    bytes[sizeof(bytes) - 1] = 0;
    M_Reject(bytes, sizeof(bytes));
}

TEST(accepts_full_segment_and_reports_absent_instruments_and_invalid_ids)
{
    uint8_t *const bytes = Mem_AllocZ(65537);
    for (size_t i = 0; i < PMD_EFFECT_COUNT; i++) {
        M_Word(bytes + i * 2, 65533);
    }
    M_Word(bytes + 254, 65534);
    bytes[65533] = 0x80;
    bytes[65535] = 0xff;
    TEST_ALLOCATOR allocator;
    TestAllocator_Init(&allocator);
    PMD_EFFECTS *bank;
    CHECK(Result_Ignore(M_Load(bytes, 65536, &allocator, &bank)));
    CHECK_EQ_INT(bank->instrument_count, 0);
    CHECK(PmdData_Instrument(bank, 0) == nullptr);
    PMD_EFFECT_PLAYER player;
    CHECK(Result_Ignore(
        PmdEffect_Init(&player, bank, &allocator.allocator, nullptr, 2)));
    CHECK(Result_Ignore(PmdEffect_Play(&player, 126)));
    CHECK(Result_Ignore(PmdEffect_Step(&player, 0, false)));
    CHECK(!player.playing);
    for (unsigned id = 127; id <= 255; id++) {
        CHECK(!Result_Ignore(PmdEffect_Play(&player, (uint8_t)id)));
        CHECK(!player.playing);
    }
    Allocator_Free(&allocator.allocator, player.work, player.work_size);
    M_Release(&allocator, bank);
    M_Reject(bytes, 65537);
    Mem_Free(bytes);
    bank = nullptr;
    CHECK(PmdData_Instrument(bank, 0) == nullptr);
}

TEST(loads_from_file_slice_and_survives_closure)
{
    char *const directory = TestDir_Prepare(__func__);
    char *const path = TestDir_Join(directory, "bank.bin");
    uint8_t bytes[17 + M_BANK_SIZE + 5] = {};
    M_Bank(bytes + 17);
    TestDir_Write(path, bytes, sizeof(bytes));
    READER file;
    CHECK(Result_Ignore(Reader_OpenFile(&file, path)));
    Reader_Seek(&file, 17);
    READER slice = Reader_Slice(&file, M_BANK_SIZE);
    TEST_ALLOCATOR allocator;
    TestAllocator_Init(&allocator);
    PMD_EFFECTS *bank;
    CHECK(Result_Ignore(
        PmdData_LoadEffects(&slice, &allocator.allocator, &bank)));
    CHECK_EQ_INT(Reader_Tell(&slice), M_BANK_SIZE);
    Reader_Close(&slice);
    Reader_Close(&file);
    CHECK(Result_Ignore(File_Remove(path)));
    CHECK_EQ_INT(PmdData_Instrument(bank, 255)[24], 55);
    CHECK(memcmp(bank->bytes, bytes + 17, M_BANK_SIZE) == 0);
    M_Release(&allocator, bank);
    Mem_Free(path);
    Mem_Free(directory);
}

TEST(rejects_failed_reader)
{
    uint8_t bytes[M_BANK_SIZE];
    M_Bank(bytes);
    READER reader;
    Reader_OpenBuffer(&reader, bytes, sizeof(bytes), nullptr);
    Reader_Seek(&reader, sizeof(bytes) + 1);
    TEST_ALLOCATOR allocator;
    TestAllocator_Init(&allocator);
    PMD_EFFECTS *bank;
    CHECK(!Result_Ignore(
        PmdData_LoadEffects(&reader, &allocator.allocator, &bank)));
    CHECK(bank == nullptr);
    CHECK_EQ_INT(allocator.live_blocks, 0);
}
