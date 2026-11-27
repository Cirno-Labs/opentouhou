#include "tests/test.h"
#include "th02/high_scores/format.h"

static uint8_t *M_Encode(
    const HIGH_SCORE_SECTION *const section, size_t *const size)
{
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator, nullptr);
    const bool success =
        Result_Ignore(HighScores_WriteSection(&writer, section));
    uint8_t *bytes = success ? Writer_TakeBuffer(&writer, size) : nullptr;
    Result_Ignore(Writer_Close(&writer));
    return bytes;
}

static RESULT M_Read(
    const uint8_t *const bytes, const size_t size,
    HIGH_SCORE_SECTION *const out)
{
    READER reader;
    Reader_OpenBuffer(&reader, bytes, size, nullptr);
    const RESULT result = HighScores_ReadSection(&reader, out);
    Reader_Close(&reader);
    return result;
}

static void M_RepairOuterSum(uint8_t *const bytes)
{
    uint32_t sum = 0;
    for (size_t i = 0; i < HIGH_SCORES_SECTION_SIZE - 4; i++) {
        sum += (uint8_t)(bytes[i] - 0x12);
    }
    for (size_t i = 0; i < 4; i++) {
        bytes[HIGH_SCORES_SECTION_SIZE - 4 + i] = sum >> (i * 8);
    }
}

TEST(normal_clears_require_no_continues_and_unlock_extra_after_all_shots)
{
    HIGH_SCORES scores;
    HighScores_Defaults(&scores);
    const HIGH_SCORES before = scores;
    for (uint8_t shot = 0; shot < 3; shot++) {
        CHECK_EQ_INT(HighScores_RecordClear(&scores, shot, false, 1), -1);
        CHECK_EQ_INT(
            HighScores_RecordClear(&scores, shot, false, UINT16_MAX), -1);
    }
    CHECK(memcmp(&scores, &before, sizeof(scores)) == 0);
    const int16_t markers[] = { 318, 118, 218 };
    for (uint8_t shot = 0; shot < 3; shot++) {
        scores.sections[shot].cleared = -1;
        CHECK_EQ_INT(HighScores_RecordClear(&scores, shot, false, 0), shot);
        CHECK_EQ_INT(scores.sections[shot].cleared, markers[shot]);
        CHECK_EQ_INT(HighScores_RecordClear(&scores, shot, false, 0), shot);
        HIGH_SCORE_CLEARS clears;
        HighScores_GetClears(&scores, &clears);
        CHECK(clears.game[shot]);
        CHECK(clears.extra_unlocked == (shot == 2));
        CHECK(
            memcmp(
                scores.sections[shot].records, before.sections[shot].records,
                sizeof(scores.sections[shot].records))
            == 0);
    }
    CHECK(
        memcmp(
            &scores.sections[3], &before.sections[3],
            2 * sizeof(HIGH_SCORE_SECTION))
        == 0);
}

TEST(extra_clear_keeps_unrelated_bits_and_round_trips_through_score_codec)
{
    HIGH_SCORES scores;
    HighScores_Defaults(&scores);
    scores.sections[3].cleared = (int16_t)0xa5f8;
    const HIGH_SCORES before = scores;
    for (uint8_t shot = 0; shot < 3; shot++) {
        CHECK_EQ_INT(
            HighScores_RecordClear(&scores, shot, true, UINT16_MAX), 3);
        CHECK_EQ_INT(
            (uint16_t)scores.sections[3].cleared,
            0xa5f8 | ((1u << (shot + 1)) - 1));
    }
    const HIGH_SCORES completed = scores;
    CHECK_EQ_INT(HighScores_RecordClear(&scores, 2, true, 0), 3);
    CHECK(memcmp(&scores, &completed, sizeof(scores)) == 0);
    HIGH_SCORE_CLEARS clears;
    HighScores_GetClears(&scores, &clears);
    CHECK(!clears.extra_unlocked);
    for (size_t shot = 0; shot < 3; shot++) {
        CHECK(clears.extra[shot]);
        CHECK(!clears.game[shot]);
    }
    for (size_t rank = 0; rank < HIGH_SCORES_SECTIONS; rank++) {
        if (rank != 3) {
            CHECK(
                memcmp(
                    &scores.sections[rank], &before.sections[rank],
                    sizeof(HIGH_SCORE_SECTION))
                == 0);
        }
    }
    size_t size;
    uint8_t *const bytes = M_Encode(&scores.sections[3], &size);
    CHECK(bytes != nullptr);
    HIGH_SCORE_SECTION decoded;
    CHECK(Result_Ignore(M_Read(bytes, size, &decoded)));
    CHECK_EQ_INT((uint16_t)decoded.cleared, 0xa5ff);
    CHECK(
        memcmp(
            decoded.records, before.sections[3].records,
            sizeof(decoded.records))
        == 0);
    Mem_Free(bytes);
}

TEST(score_sections_keep_original_arrays_obfuscation_and_wrapped_score_sum)
{
    HIGH_SCORE_SECTION section = { .cleared = 318 };
    section.records[0].score = section.records[1].score = INT32_MAX;
    section.records[2].score = -1;
    section.records[0].name_glyphs[0] = 0xfe;
    section.records[0].name_glyphs[1] = 0xff;
    section.records[0].stage = 127;
    section.records[0].year = 2026;
    section.records[0].day = 21;
    section.records[0].month = 11;
    section.records[0].shot_type = 2;
    const HIGH_SCORE_SECTION before = section;
    size_t size;
    uint8_t *const bytes = M_Encode(&section, &size);
    CHECK(bytes != nullptr);
    CHECK_EQ_INT(size, HIGH_SCORES_SECTION_SIZE);
    CHECK_EQ_INT(bytes[0], 0x50);
    CHECK_EQ_INT(bytes[1], 0x13);
    CHECK_EQ_INT(bytes[42], 0x0f);
    CHECK_EQ_INT(bytes[43], 0x11);
    CHECK_EQ_INT(bytes[116], 0x10);
    CHECK_EQ_INT(bytes[128], (2026 & 255) + 0x12);
    CHECK_EQ_INT(bytes[130], 21 + 0x12);
    CHECK_EQ_INT(bytes[131], 11 + 0x12);
    CHECK_EQ_INT(bytes[168], 2 + 0x12);
    CHECK(memcmp(&section, &before, sizeof(section)) == 0);
    HIGH_SCORE_SECTION decoded;
    CHECK(Result_Ignore(M_Read(bytes, size, &decoded)));
    CHECK_EQ_INT(decoded.cleared, 318);
    CHECK_EQ_INT(decoded.records[0].score, INT32_MAX);
    CHECK_EQ_INT(decoded.records[2].score, -1);
    CHECK_EQ_INT(decoded.records[0].name_glyphs[1], 0xff);
    CHECK_EQ_INT(decoded.records[0].name_glyphs[6], 0);
    CHECK_EQ_INT(decoded.records[0].year, 2026);
    CHECK_EQ_INT(decoded.records[0].day, 21);
    CHECK_EQ_INT(decoded.records[0].month, 11);
    Mem_Free(bytes);
}

TEST(score_integrity_checks_each_inner_sum_even_when_outer_sum_is_valid)
{
    HIGH_SCORE_SECTION section = {};
    size_t size;
    uint8_t *const bytes = M_Encode(&section, &size);
    const size_t offsets[] = { 42, 116, 127, 178 };
    for (size_t i = 0; i < ARRAY_SIZE(offsets); i++) {
        uint8_t changed[HIGH_SCORES_SECTION_SIZE];
        memcpy(changed, bytes, sizeof(changed));
        changed[offsets[i]]++;
        if (i < 3) {
            M_RepairOuterSum(changed);
        }
        HIGH_SCORE_SECTION out;
        memset(&out, 0xa5, sizeof(out));
        const HIGH_SCORE_SECTION before = out;
        CHECK(!Result_Ignore(M_Read(changed, sizeof(changed), &out)));
        CHECK(memcmp(&out, &before, sizeof(out)) == 0);
    }
    Mem_Free(bytes);
}

TEST(
    score_file_reads_five_sections_leaves_trailing_bytes_and_rejects_truncation)
{
    HIGH_SCORES scores = {};
    for (size_t i = 0; i < HIGH_SCORES_SECTIONS; i++) {
        scores.sections[i].records[0].score = 1000 + (int32_t)i;
    }
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator, nullptr);
    CHECK(Result_Ignore(HighScores_Write(&writer, &scores)));
    Writer_WriteU8(&writer, 0x7a);
    size_t size;
    uint8_t *const bytes = Writer_TakeBuffer(&writer, &size);
    CHECK(Result_Ignore(Writer_Close(&writer)));
    CHECK_EQ_INT(size, HIGH_SCORES_SECTIONS * HIGH_SCORES_SECTION_SIZE + 1);
    READER reader;
    Reader_OpenBuffer(&reader, bytes, size, nullptr);
    HIGH_SCORES out;
    CHECK(Result_Ignore(HighScores_Read(&reader, &out)));
    CHECK_EQ_INT(Reader_Tell(&reader), size - 1);
    CHECK_EQ_INT(Reader_ReadU8(&reader), 0x7a);
    CHECK_EQ_INT(out.sections[4].records[0].score, 1004);
    Reader_Close(&reader);
    Reader_OpenBuffer(&reader, bytes, size - 2, nullptr);
    memset(&out, 0xa5, sizeof(out));
    const HIGH_SCORES before = out;
    CHECK(!Result_Ignore(HighScores_Read(&reader, &out)));
    CHECK(memcmp(&out, &before, sizeof(out)) == 0);
    Reader_Close(&reader);
    Mem_Free(bytes);
}

TEST(clear_flags_use_shot_specific_rank_fields_and_exact_clear_constants)
{
    HIGH_SCORES scores = {};
    scores.sections[0].cleared = 318;
    scores.sections[1].cleared = 118;
    scores.sections[2].cleared = 218;
    scores.sections[3].cleared = -1;
    scores.sections[4].cleared = 0;
    HIGH_SCORE_CLEARS clears;
    HighScores_GetClears(&scores, &clears);
    CHECK(clears.extra_unlocked);
    CHECK(clears.game[0] && clears.game[1] && clears.game[2]);
    CHECK(clears.extra[0] && clears.extra[1] && clears.extra[2]);
    scores.sections[1].cleared = 119;
    scores.sections[3].cleared = 0x102;
    scores.sections[4].cleared = -1;
    HighScores_GetClears(&scores, &clears);
    CHECK(!clears.extra_unlocked);
    CHECK(clears.game[0] && !clears.game[1] && clears.game[2]);
    CHECK(!clears.extra[0] && clears.extra[1] && !clears.extra[2]);
}

TEST(factory_scores_reset_all_five_tables_and_emit_identical_valid_sections)
{
    HIGH_SCORES scores;
    memset(&scores, 0xa5, sizeof(scores));
    HighScores_Defaults(&scores);
    CHECK_EQ_INT(scores.sections[0].records[0].score, 10000);
    CHECK_EQ_INT(scores.sections[4].records[9].score, 1000);
    CHECK_EQ_INT(scores.sections[4].records[9].stage, 1);
    CHECK_EQ_INT(scores.sections[0].records[0].year, 1900);
    const uint8_t name[] = { 0xda, 0xda, 0xda, 0xda, 0xda, 0xda, 0 };
    CHECK(
        memcmp(scores.sections[0].records[0].name_glyphs, name, sizeof(name))
        == 0);
    HIGH_SCORE_CLEARS clears;
    HighScores_GetClears(&scores, &clears);
    CHECK(!clears.extra_unlocked);
    for (size_t shot = 0; shot < 3; shot++) {
        CHECK(!clears.game[shot] && !clears.extra[shot]);
    }
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator, nullptr);
    CHECK(Result_Ignore(HighScores_Write(&writer, &scores)));
    size_t size;
    uint8_t *const bytes = Writer_TakeBuffer(&writer, &size);
    CHECK(Result_Ignore(Writer_Close(&writer)));
    CHECK_EQ_INT(size, HIGH_SCORES_SECTIONS * HIGH_SCORES_SECTION_SIZE);
    for (size_t rank = 1; rank < HIGH_SCORES_SECTIONS; rank++) {
        CHECK(
            memcmp(
                bytes, bytes + rank * HIGH_SCORES_SECTION_SIZE,
                HIGH_SCORES_SECTION_SIZE)
            == 0);
    }
    READER reader;
    Reader_OpenBuffer(&reader, bytes, size, nullptr);
    HIGH_SCORES loaded;
    CHECK(Result_Ignore(HighScores_Read(&reader, &loaded)));
    CHECK_EQ_INT(loaded.sections[4].records[9].score, 1000);
    CHECK_EQ_INT(loaded.sections[4].records[9].stage, 1);
    Reader_Close(&reader);
    Mem_Free(bytes);
}
