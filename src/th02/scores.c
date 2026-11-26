#include "th02/scores.h"

#include "core/base/assert.h"

#include <string.h>

#define PAYLOAD_SIZE (SCORES_SECTION_SIZE - 4)

static const int16_t m_ClearMarkers[3] = { 318, 118, 218 };

static uint32_t M_ScoreSum(const SCORE_SECTION *const section)
{
    uint32_t sum = 0;
    for (size_t i = 0; i < SCORES_PLACES; i++) {
        sum += (uint32_t)section->records[i].score;
    }
    return sum;
}

static uint8_t M_NameSum(const SCORE_SECTION *const section)
{
    uint8_t sum = 0;
    for (size_t i = 0; i < SCORES_PLACES; i++) {
        sum = (uint8_t)(sum + section->records[i].name_glyphs[0]);
    }
    return sum;
}

static uint8_t M_StageSum(const SCORE_SECTION *const section)
{
    uint8_t sum = 0;
    for (size_t i = 0; i < SCORES_PLACES; i++) {
        sum = (uint8_t)(sum + section->records[i].stage);
    }
    return sum;
}

void Scores_Defaults(SCORES *const scores)
{
    memset(scores, 0, sizeof(*scores));
    for (size_t rank = 0; rank < SCORES_SECTIONS; rank++) {
        for (size_t place = 0; place < SCORES_PLACES; place++) {
            SCORE_RECORD *const record = &scores->sections[rank].records[place];
            record->score = 10000 - (int32_t)place * 1000;
            memset(record->name_glyphs, 0xda, SCORES_NAME_GLYPHS - 1);
            record->stage = 5 - place / 2;
            record->shot_type = 1;
            record->year = 1900;
            record->day = record->month = 1;
        }
    }
}

RESULT Scores_ReadSection(READER *const reader, SCORE_SECTION *const out)
{
    MUST(Reader_Result(reader));
    uint8_t bytes[PAYLOAD_SIZE];
    Reader_ReadBytes(reader, bytes, sizeof(bytes));
    const uint32_t expected_section = Reader_ReadU32(reader);
    MUST(Reader_Result(reader));
    uint32_t sum = 0;
    for (size_t i = 0; i < sizeof(bytes); i++) {
        bytes[i] = (uint8_t)(bytes[i] - 0x12);
        sum += bytes[i];
    }
    READER payload;
    Reader_OpenBuffer(&payload, bytes, sizeof(bytes), nullptr);
    SCORE_SECTION section = {};
    section.cleared = Reader_ReadI16(&payload);
    for (size_t i = 0; i < SCORES_PLACES; i++) {
        section.records[i].score = Reader_ReadI32(&payload);
    }
    const uint32_t expected_score = Reader_ReadU32(&payload);
    for (size_t i = 0; i < SCORES_PLACES; i++) {
        Reader_ReadBytes(
            &payload, section.records[i].name_glyphs, SCORES_NAME_GLYPHS);
    }
    const uint8_t expected_name = Reader_ReadU8(&payload);
    for (size_t i = 0; i < SCORES_PLACES; i++) {
        section.records[i].stage = Reader_ReadU8(&payload);
    }
    const uint8_t expected_stage = Reader_ReadU8(&payload);
    for (size_t i = 0; i < SCORES_PLACES; i++) {
        SCORE_RECORD *const record = &section.records[i];
        record->year = Reader_ReadI16(&payload);
        record->day = Reader_ReadU8(&payload);
        record->month = Reader_ReadU8(&payload);
    }
    for (size_t i = 0; i < SCORES_PLACES; i++) {
        section.records[i].shot_type = Reader_ReadU8(&payload);
    }
    const RESULT result = Reader_Result(&payload);
    Reader_Close(&payload);
    MUST(result);
    FAIL_IF(
        sum != expected_section || M_ScoreSum(&section) != expected_score
            || M_NameSum(&section) != expected_name
            || M_StageSum(&section) != expected_stage,
        "Invalid HUUHI.DAT section checksums");
    *out = section;
    return OK;
}

RESULT Scores_WriteSection(
    WRITER *const writer, const SCORE_SECTION *const section)
{
    MUST(Writer_Result(writer));
    WRITER payload;
    Writer_OpenBuffer(&payload, &g_HeapAllocator, nullptr);
    Writer_WriteI16(&payload, section->cleared);
    for (size_t i = 0; i < SCORES_PLACES; i++) {
        Writer_WriteI32(&payload, section->records[i].score);
    }
    Writer_WriteU32(&payload, M_ScoreSum(section));
    for (size_t i = 0; i < SCORES_PLACES; i++) {
        Writer_WriteBytes(
            &payload, section->records[i].name_glyphs, SCORES_NAME_GLYPHS);
    }
    Writer_WriteU8(&payload, M_NameSum(section));
    for (size_t i = 0; i < SCORES_PLACES; i++) {
        Writer_WriteU8(&payload, section->records[i].stage);
    }
    Writer_WriteU8(&payload, M_StageSum(section));
    for (size_t i = 0; i < SCORES_PLACES; i++) {
        const SCORE_RECORD *const record = &section->records[i];
        Writer_WriteI16(&payload, record->year);
        Writer_WriteU8(&payload, record->day);
        Writer_WriteU8(&payload, record->month);
    }
    for (size_t i = 0; i < SCORES_PLACES; i++) {
        Writer_WriteU8(&payload, section->records[i].shot_type);
    }
    size_t size;
    uint8_t *const bytes = Writer_TakeBuffer(&payload, &size);
    const RESULT result = Writer_Close(&payload);
    if (!IS_OK(result)) {
        Mem_Free(bytes);
        return result;
    }
    uint32_t sum = 0;
    for (size_t i = 0; i < size; i++) {
        sum += bytes[i];
        bytes[i] = (uint8_t)(bytes[i] + 0x12);
    }
    Writer_WriteBytes(writer, bytes, size);
    Writer_WriteU32(writer, sum);
    Mem_Free(bytes);
    return Writer_Result(writer);
}

RESULT Scores_Read(READER *const reader, SCORES *const out)
{
    SCORES scores = {};
    for (size_t i = 0; i < SCORES_SECTIONS; i++) {
        MUST(Scores_ReadSection(reader, &scores.sections[i]));
    }
    *out = scores;
    return OK;
}

RESULT Scores_Write(WRITER *const writer, const SCORES *const scores)
{
    for (size_t i = 0; i < SCORES_SECTIONS; i++) {
        MUST(Scores_WriteSection(writer, &scores->sections[i]));
    }
    return OK;
}

void Scores_GetClears(const SCORES *const scores, SCORE_CLEARS *const out)
{
    out->extra_unlocked = true;
    for (size_t i = 0; i < 3; i++) {
        out->game[i] = scores->sections[i].cleared == m_ClearMarkers[i];
        out->extra[i] =
            ((uint16_t)scores->sections[3].cleared & (1u << i)) != 0;
        out->extra_unlocked &= out->game[i];
    }
}

int8_t Scores_RecordClear(
    SCORES *const scores, const uint8_t shot_type, const bool extra,
    const uint16_t continues_used)
{
    ASSERT(shot_type < 3);
    if (extra) {
        scores->sections[3].cleared =
            (int16_t)((uint16_t)scores->sections[3].cleared
                      | (1u << shot_type));
        return 3;
    }
    if (continues_used != 0) {
        return -1;
    }
    scores->sections[shot_type].cleared = m_ClearMarkers[shot_type];
    return shot_type;
}
