#include "th06/replay.h"

#include "test.h"

#include "th06/replay/format.h"

#include "core/base/memory.h"

#define M_FIXTURE_SIZE 120

static void M_PutU16(
    uint8_t *const bytes, const size_t offset, const uint16_t value)
{
    bytes[offset] = (uint8_t)value;
    bytes[offset + 1] = (uint8_t)(value >> 8);
}

static void M_PutU32(
    uint8_t *const bytes, const size_t offset, const uint32_t value)
{
    for (int32_t i = 0; i < 4; i++) {
        bytes[offset + (size_t)i] = (uint8_t)(value >> (8 * i));
    }
}

static uint32_t M_U32(const uint8_t *const bytes, const size_t offset)
{
    return (uint32_t)bytes[offset] | (uint32_t)bytes[offset + 1] << 8
        | (uint32_t)bytes[offset + 2] << 16 | (uint32_t)bytes[offset + 3] << 24;
}

static float M_F32(const uint8_t *const bytes, const size_t offset)
{
    const uint32_t bits = M_U32(bytes, offset);
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static void M_Plain(uint8_t *const bytes, const size_t size)
{
    memset(bytes, 0, size);
    memcpy(bytes, "T6RP", 4);
    M_PutU16(bytes, 4, 0x102);
    bytes[6] = 3;
    bytes[7] = 2;
    bytes[12] = 0x71;
    bytes[13] = 0x39;
    bytes[14] = 0x63;
    bytes[15] = 0x81;
    memcpy(bytes + 16, "11/10/26", 8);
    memcpy(bytes + 25, "SYNTH", 5);
    M_PutU32(bytes, 36, 123456);
    M_PutU32(bytes, 44, 0x41a00000);
    M_PutU32(bytes, 52, 80);
    M_PutU32(bytes, 80, 98765);
    M_PutU16(bytes, 84, 0x1234);
    M_PutU16(bytes, 86, 19);
    bytes[88] = 64;
    bytes[89] = 3;
    bytes[90] = 2;
    bytes[91] = 17;
    bytes[92] = 7;
    M_PutU16(bytes, 108, 0x101);
    M_PutU32(bytes, 112, 9999999);
}

static void M_Encrypt(uint8_t *const bytes, const size_t size)
{
    uint32_t checksum = 0x3f000318;
    for (size_t i = 14; i < size; i++) {
        checksum += bytes[i];
    }
    M_PutU32(bytes, 8, checksum);
    for (size_t i = 15; i < size; i++) {
        bytes[i] = (uint8_t)(bytes[i] + bytes[14] + (i - 15) * 7);
    }
}

static REPLAY_FILE *M_Read(uint8_t *const bytes, const size_t size)
{
    READER reader;
    Reader_OpenBuffer(&reader, bytes, size);
    REPLAY_FILE *file = nullptr;
    CHECK(Result_Log(Replay_Read(&reader, &file), LOG_LEVEL_ERROR));
    return file;
}

static void M_Reject(uint8_t *const bytes, const size_t size)
{
    READER reader;
    Reader_OpenBuffer(&reader, bytes, size);
    REPLAY_FILE *file = nullptr;
    CHECK(!Result_Ignore(Replay_Read(&reader, &file)));
    CHECK(file == nullptr);
    Replay_Free(file);
}

static RESULT M_Write(
    WRITER *const writer, const REPLAY_FILE *const file,
    const REPLAY_SAVE_INFO *const info, const uint8_t random[4])
{
    REPLAY_FORMAT_SAVE prepared;
    MUST(ReplayFormat_Prepare(info, &prepared));
    return ReplayFormat_Write(writer, file, &prepared, random);
}

TEST(reads_an_independent_encrypted_fixture)
{
    uint8_t bytes[M_FIXTURE_SIZE];
    M_Plain(bytes, sizeof(bytes));
    M_Encrypt(bytes, sizeof(bytes));
    const uint8_t first_encrypted = bytes[15];
    REPLAY_FILE *const file = M_Read(bytes, sizeof(bytes));
    if (file == nullptr) {
        return;
    }
    CHECK_EQ_INT(bytes[15], first_encrypted);
    CHECK_EQ_INT(file->shot, 3);
    CHECK_EQ_INT(file->difficulty, 2);
    CHECK_EQ_STR(file->date, "11/10/26");
    CHECK_EQ_STR(file->name, "SYNTH");
    CHECK_EQ_INT(file->score, 123456);
    CHECK(file->slowdown == 20.0f);
    CHECK(file->stages[0] == nullptr);
    CHECK(file->stages[2] == nullptr);
    const REPLAY_STAGE *const stage = file->stages[1];
    CHECK(stage != nullptr);
    CHECK_EQ_INT(stage->score, 98765);
    CHECK_EQ_INT(stage->rng_seed, 0x1234);
    CHECK_EQ_INT(stage->point_items_total, 19);
    CHECK_EQ_INT(stage->power, 64);
    CHECK_EQ_INT(stage->lives, 3);
    CHECK_EQ_INT(stage->bombs, 2);
    CHECK_EQ_INT(stage->rank, 17);
    CHECK_EQ_INT(stage->power_item_combo, 7);
    CHECK_EQ_INT(stage->frame_count, 3);
    CHECK_EQ_INT(stage->frames[0].frame, 0);
    CHECK_EQ_INT(stage->frames[1].frame, 0);
    CHECK_EQ_INT(stage->frames[1].buttons, 0x101);
    CHECK_EQ_INT(stage->frames[2].frame, 9999999);
    Replay_Free(file);
}

TEST(rejects_non_ascii_dates_on_read_and_save)
{
    uint8_t bytes[M_FIXTURE_SIZE];
    M_Plain(bytes, sizeof(bytes));
    bytes[16] = 0xff;
    M_Encrypt(bytes, sizeof(bytes));
    M_Reject(bytes, sizeof(bytes));
    const REPLAY_SAVE_INFO info = { "Test",
                                    "\xff"
                                    "1/10/26",
                                    1.0f, 1.0f };
    REPLAY_FORMAT_SAVE prepared;
    CHECK(!Result_Ignore(ReplayFormat_Prepare(&info, &prepared)));
}

TEST(decodes_cp932_names)
{
    uint8_t bytes[M_FIXTURE_SIZE];
    M_Plain(bytes, sizeof(bytes));
    memset(bytes + 25, 0, 9);
    bytes[25] = 0x82;
    bytes[26] = 0xa0;
    M_Encrypt(bytes, sizeof(bytes));
    REPLAY_FILE *const file = M_Read(bytes, sizeof(bytes));
    if (file != nullptr) {
        CHECK_EQ_STR(file->name, "あ");
    }
    Replay_Free(file);
}

TEST(preserves_entries_after_an_earlier_end_marker)
{
    uint8_t bytes[M_FIXTURE_SIZE + 16];
    M_Plain(bytes, sizeof(bytes));
    M_PutU32(bytes, 120, 23);
    M_PutU32(bytes, 128, 9999999);
    M_Encrypt(bytes, sizeof(bytes));
    REPLAY_FILE *const file = M_Read(bytes, sizeof(bytes));
    if (file != nullptr) {
        CHECK_EQ_INT(file->stages[1]->frame_count, 5);
        CHECK_EQ_INT(file->stages[1]->frames[3].frame, 23);
    }
    Replay_Free(file);
}

TEST(rejects_short_headers_and_truncated_stages)
{
    uint8_t bytes[M_FIXTURE_SIZE];
    M_Plain(bytes, sizeof(bytes));
    M_Encrypt(bytes, sizeof(bytes));
    for (size_t size = 0; size < 80; size++) {
        M_Reject(bytes, size);
    }
    M_Plain(bytes, sizeof(bytes));
    M_Encrypt(bytes, sizeof(bytes) - 1);
    M_Reject(bytes, sizeof(bytes) - 1);
}

TEST(rejects_bad_magic_version_checksum_and_indices)
{
    const size_t offsets[] = { 0, 4, 6, 7 };
    const uint8_t values[] = { 'X', 0, 4, 5 };
    for (size_t i = 0; i < sizeof(offsets) / sizeof(offsets[0]); i++) {
        uint8_t bytes[M_FIXTURE_SIZE];
        M_Plain(bytes, sizeof(bytes));
        bytes[offsets[i]] = values[i];
        M_Encrypt(bytes, sizeof(bytes));
        M_Reject(bytes, sizeof(bytes));
    }
    uint8_t bytes[M_FIXTURE_SIZE];
    M_Plain(bytes, sizeof(bytes));
    M_Encrypt(bytes, sizeof(bytes));
    bytes[100]++;
    M_Reject(bytes, sizeof(bytes));
}

TEST(rejects_offsets_outside_the_file_or_inside_a_stage)
{
    const uint32_t offsets[] = { 79, M_FIXTURE_SIZE, UINT32_MAX };
    for (size_t i = 0; i < sizeof(offsets) / sizeof(offsets[0]); i++) {
        uint8_t bytes[M_FIXTURE_SIZE];
        M_Plain(bytes, sizeof(bytes));
        M_PutU32(bytes, 52, offsets[i]);
        M_Encrypt(bytes, sizeof(bytes));
        M_Reject(bytes, sizeof(bytes));
    }
    uint8_t bytes[M_FIXTURE_SIZE];
    M_Plain(bytes, sizeof(bytes));
    M_PutU32(bytes, 56, 84);
    M_Encrypt(bytes, sizeof(bytes));
    M_Reject(bytes, sizeof(bytes));
}

TEST(rejects_missing_stages_bad_names_and_invalid_frame_sequences)
{
    const size_t offsets[] = { 52, 25, 104, 112 };
    const uint32_t values[] = { 0, 0x82, UINT32_MAX, 55 };
    for (size_t i = 0; i < sizeof(offsets) / sizeof(offsets[0]); i++) {
        uint8_t bytes[M_FIXTURE_SIZE];
        M_Plain(bytes, sizeof(bytes));
        M_PutU32(bytes, offsets[i], values[i]);
        M_Encrypt(bytes, sizeof(bytes));
        M_Reject(bytes, sizeof(bytes));
    }
}

TEST(accepts_an_absent_first_stage_and_out_of_order_stage_offsets)
{
    uint8_t bytes[M_FIXTURE_SIZE * 2 - 80];
    M_Plain(bytes, sizeof(bytes));
    memcpy(bytes + 120, bytes + 80, 40);
    M_PutU32(bytes, 52, 0);
    M_PutU32(bytes, 56, 120);
    M_PutU32(bytes, 60, 80);
    M_Encrypt(bytes, sizeof(bytes));
    REPLAY_FILE *const file = M_Read(bytes, sizeof(bytes));
    if (file != nullptr) {
        CHECK(file->stages[1] == nullptr);
        CHECK_EQ_INT(file->stages[2]->frame_count, 3);
        CHECK_EQ_INT(file->stages[3]->frame_count, 3);
    }
    Replay_Free(file);
}

TEST(rejects_stage_values_that_are_unsafe_for_gameplay)
{
    const size_t offsets[] = { 80, 80, 89, 90 };
    const uint32_t values[] = { UINT32_MAX, 1900000000, 9, 9 };
    for (size_t i = 0; i < sizeof(offsets) / sizeof(offsets[0]); i++) {
        uint8_t bytes[M_FIXTURE_SIZE];
        M_Plain(bytes, sizeof(bytes));
        M_PutU32(bytes, offsets[i], values[i]);
        M_Encrypt(bytes, sizeof(bytes));
        M_Reject(bytes, sizeof(bytes));
    }
    uint8_t bytes[M_FIXTURE_SIZE];
    M_Plain(bytes, sizeof(bytes));
    M_PutU32(bytes, 80, 1899999999);
    bytes[89] = 8;
    bytes[90] = 8;
    M_Encrypt(bytes, sizeof(bytes));
    REPLAY_FILE *const file = M_Read(bytes, sizeof(bytes));
    Replay_Free(file);
}

TEST(writes_the_original_wire_layout_and_encryption)
{
    REPLAY_FRAME frames[] = { { 0, 0 }, { 5, 0x81 }, { 9999999, 0 } };
    REPLAY_STAGE stage = {
        .score = 12345,
        .rng_seed = 0xabcd,
        .point_items_total = 123,
        .power = 128,
        .lives = 4,
        .bombs = 3,
        .rank = 16,
        .power_item_combo = 30,
        .frames = frames,
        .frame_count = 3,
    };
    const REPLAY_FILE file = {
        .shot = 2,
        .difficulty = 3,
        .score = 12340,
        .stages = { [3] = &stage },
    };
    const REPLAY_SAVE_INFO info = {
        .name = "あ123456",
        .date = "11/10/26",
        .achieved_frames = 75.0f,
        .expected_frames = 100.0f,
    };
    const uint8_t random[] = { 99, 88, 77, 66 };
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator);
    CHECK(Result_Log(M_Write(&writer, &file, &info, random), LOG_LEVEL_ERROR));
    size_t size;
    uint8_t *const bytes = Writer_TakeBuffer(&writer, &size);
    CHECK(Result_Ignore(Writer_Close(&writer)));
    CHECK_EQ_INT(size, 120);
    CHECK(memcmp(bytes, "T6RP", 4) == 0);
    CHECK_EQ_INT(bytes[12], 77);
    CHECK_EQ_INT(bytes[13], 66);
    CHECK_EQ_INT(bytes[14], 99);
    CHECK_EQ_INT(bytes[15], (uint8_t)(88 + 99));
    const uint32_t expected_checksum = M_U32(bytes, 8);
    for (size_t i = 15; i < size; i++) {
        bytes[i] = (uint8_t)(bytes[i] - bytes[14] - (i - 15) * 7);
    }
    uint32_t checksum = 0x3f000318;
    for (size_t i = 14; i < size; i++) {
        checksum += bytes[i];
    }
    CHECK_EQ_INT(checksum, expected_checksum);
    CHECK_EQ_INT(M_U32(bytes, 36), 12340);
    CHECK(M_F32(bytes, 40) == 50.0f + 1.12f);
    CHECK(M_F32(bytes, 44) == 50.0f);
    CHECK(M_F32(bytes, 48) == 50.0f + 2.34f);
    CHECK_EQ_INT(M_U32(bytes, 52), 0);
    CHECK_EQ_INT(M_U32(bytes, 60), 80);
    CHECK_EQ_INT(M_U32(bytes, 80), 12345);
    CHECK_EQ_INT(bytes[84], 0xcd);
    CHECK_EQ_INT(bytes[85], 0xab);
    CHECK_EQ_INT(bytes[25], 0x82);
    CHECK_EQ_INT(bytes[26], 0xa0);
    CHECK_EQ_INT(bytes[33], 0);
    CHECK_EQ_INT(M_U32(bytes, 104), 5);
    CHECK_EQ_INT(bytes[108], 0x81);
    M_Encrypt(bytes, size);
    REPLAY_FILE *const decoded = M_Read(bytes, size);
    if (decoded != nullptr) {
        CHECK_EQ_STR(decoded->name, info.name);
        CHECK_EQ_INT(decoded->stages[3]->rng_seed, stage.rng_seed);
    }
    Replay_Free(decoded);
    Mem_Free(bytes);
}

TEST(rejects_names_that_do_not_fit_original_files)
{
    const REPLAY_FILE file = {};
    REPLAY_SAVE_INFO info = {
        .date = "11/10/26",
        .achieved_frames = 1.0f,
        .expected_frames = 1.0f,
    };
    const char *const names[] = { "123456789", "😀" };
    const uint8_t random[] = { 64, 0, 0, 0 };
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        WRITER writer;
        Writer_OpenBuffer(&writer, &g_HeapAllocator);
        info.name = names[i];
        CHECK(!Result_Ignore(M_Write(&writer, &file, &info, random)));
        CHECK_EQ_INT(Writer_Tell(&writer), 0);
        CHECK(Result_Ignore(Writer_Close(&writer)));
    }
}

TEST(prepares_slowdown_with_the_original_clamps)
{
    REPLAY_SAVE_INFO info = {
        .name = "SYNTH",
        .date = "11/10/26",
        .expected_frames = 100.0f,
    };
    const float achieved[] = { 0.0f, 50.0f, 100.0f, 200.0f };
    const float expected[] = { 100.0f, 100.0f, 0.0f, 0.0f };
    for (size_t i = 0; i < sizeof(achieved) / sizeof(achieved[0]); i++) {
        REPLAY_FORMAT_SAVE prepared;
        info.achieved_frames = achieved[i];
        CHECK(Result_Ignore(ReplayFormat_Prepare(&info, &prepared)));
        CHECK(prepared.slowdown == expected[i]);
    }
    REPLAY_FORMAT_SAVE prepared;
    info.date = "bad";
    CHECK(!Result_Ignore(ReplayFormat_Prepare(&info, &prepared)));
    CHECK(!Result_Ignore(ReplayFormat_Prepare(nullptr, &prepared)));
}

TEST(summary_takes_lives_and_bombs_from_the_first_stage)
{
    REPLAY_STAGE second = { .score = 100, .lives = 4, .bombs = 1 };
    REPLAY_STAGE fourth = { .score = 300, .lives = 1, .bombs = 2 };
    REPLAY_FILE file = {
        .shot = 3,
        .difficulty = 2,
        .score = 400,
        .slowdown = 1.5f,
        .stages = { [2] = &second, [4] = &fourth },
    };
    memcpy(file.date, "01/02/03", 9);
    memcpy(file.name, "NAME", 5);
    REPLAY_SUMMARY summary;
    Replay_Summarize(&file, &summary);
    CHECK_EQ_INT(summary.shot, 3);
    CHECK_EQ_INT(summary.difficulty, 2);
    CHECK_EQ_INT(summary.score, 400);
    CHECK(summary.slowdown == 1.5f);
    CHECK_EQ_STR(summary.date, "01/02/03");
    CHECK_EQ_STR(summary.name, "NAME");
    CHECK(!summary.stages[1]);
    CHECK(summary.stages[2]);
    CHECK(!summary.stages[3]);
    CHECK(summary.stages[4]);
    CHECK_EQ_INT(summary.stage_scores[4], 300);
    CHECK_EQ_INT(summary.lives, 4);
    CHECK_EQ_INT(summary.bombs, 1);
}
