// SPDX-License-Identifier: GPL-3.0-only

#include "format.h"

#include "th06/replay/file.h"

#include "core/base/cp932.h"
#include "core/base/memory.h"
#include "core/io/reader.h"

#include <limits.h>
#include <string.h>

#define M_HEADER_SIZE 0x50
#define M_STAGE_HEADER_SIZE 16
#define M_FRAME_SIZE 8
#define M_END_FRAME 9999999
#define M_CHECKSUM_START 0x3f000318u

static RESULT M_ReadStage(READER *const reader, REPLAY_STAGE *const stage)
{
    FAIL_IF(
        Reader_Remaining(reader) < M_STAGE_HEADER_SIZE + 2 * M_FRAME_SIZE,
        "replay stage is too short");
    FAIL_IF(
        (Reader_Remaining(reader) - M_STAGE_HEADER_SIZE) % M_FRAME_SIZE != 0,
        "replay stage has a partial frame");
    stage->score = Reader_ReadI32(reader);
    FAIL_IF(
        stage->score < 0 || stage->score >= 1900000000,
        "replay stage score exceeds the extend range");
    stage->rng_seed = Reader_ReadU16(reader);
    stage->point_items_total = Reader_ReadU16(reader);
    stage->power = Reader_ReadU8(reader);
    stage->lives = Reader_ReadU8(reader);
    stage->bombs = Reader_ReadU8(reader);
    FAIL_IF(
        stage->lives > 8 || stage->bombs > 8,
        "replay stage lives or bombs exceed eight");
    stage->rank = Reader_ReadU8(reader);
    stage->power_item_combo = Reader_ReadU8(reader);
    Reader_Skip(reader, 3);
    stage->frame_count = (int32_t)(Reader_Remaining(reader) / M_FRAME_SIZE);
    stage->frames =
        Mem_AllocArr((size_t)stage->frame_count, sizeof(REPLAY_FRAME));
    bool ended = false;
    int32_t previous = 0;
    for (int32_t i = 0; i < stage->frame_count; i++) {
        REPLAY_FRAME *const frame = &stage->frames[i];
        frame->frame = Reader_ReadI32(reader);
        frame->buttons = Reader_ReadU16(reader);
        Reader_Skip(reader, 2);
        if (!ended) {
            FAIL_IF(
                frame->frame < previous || frame->frame > M_END_FRAME,
                "replay frames are out of order");
            previous = frame->frame;
            if (frame->frame == M_END_FRAME) {
                FAIL_IF(i == 0, "replay stage starts at its end marker");
                ended = true;
            }
        }
    }
    FAIL_IF(!ended, "replay stage has no end marker");
    FAIL_IF(
        stage->frames[stage->frame_count - 1].frame != M_END_FRAME,
        "replay stage does not end with its end marker");
    return Reader_Result(reader);
}

static RESULT M_Read(READER *const reader, REPLAY_FILE *const file)
{
    char magic[4];
    Reader_ReadBytes(reader, magic, sizeof(magic));
    FAIL_IF(memcmp(magic, "T6RP", sizeof(magic)) != 0, "invalid replay magic");
    const uint16_t version = Reader_ReadU16(reader);
    FAIL_IF(version != 0x102, "unsupported replay version 0x%x", version);
    file->shot = Reader_ReadU8(reader);
    file->difficulty = Reader_ReadU8(reader);
    FAIL_IF(file->shot > 3, "invalid replay shot %u", file->shot);
    FAIL_IF(
        file->difficulty > 4, "invalid replay difficulty %u", file->difficulty);
    Reader_Skip(reader, 8);
    Reader_ReadBytes(reader, file->date, 8);
    file->date[8] = '\0';
    for (int32_t i = 0; i < 8; i++) {
        FAIL_IF((uint8_t)file->date[i] > 0x7f, "replay date is not ASCII");
    }
    Reader_Skip(reader, 1);
    char name_bytes[9];
    Reader_ReadBytes(reader, name_bytes, sizeof(name_bytes));
    size_t name_size = 0;
    while (name_size < 8 && name_bytes[name_size] != '\0') {
        name_size++;
    }
    char *name;
    MUST(Cp932_Decode(name_bytes, name_size, &name), "replay name");
    memcpy(file->name, name, strlen(name) + 1);
    Mem_Free(name);
    Reader_Skip(reader, 2);
    file->score = Reader_ReadU32(reader);
    Reader_Skip(reader, 4);
    file->slowdown = Reader_ReadF32(reader);
    Reader_Skip(reader, 4);
    uint32_t offsets[8] = {};
    for (int32_t i = 1; i <= 7; i++) {
        offsets[i] = Reader_ReadU32(reader);
        FAIL_IF(
            offsets[i] != 0
                && (offsets[i] < M_HEADER_SIZE
                    || offsets[i] >= Reader_Size(reader)),
            "replay stage %d is outside the file", i);
    }
    MUST(Reader_Result(reader));
    bool any_stage = false;
    for (int32_t i = 1; i <= 7; i++) {
        if (offsets[i] == 0) {
            continue;
        }
        any_stage = true;
        size_t end = Reader_Size(reader);
        for (int32_t j = 1; j <= 7; j++) {
            if (offsets[j] > offsets[i] && offsets[j] < end) {
                end = offsets[j];
            }
        }
        Reader_Seek(reader, offsets[i]);
        READER stage_reader = Reader_Slice(reader, end - offsets[i]);
        file->stages[i] = Mem_AllocZ(sizeof(REPLAY_STAGE));
        MUST(M_ReadStage(&stage_reader, file->stages[i]), "replay stage %d", i);
    }
    FAIL_IF(!any_stage, "replay has no stages");
    return OK;
}

static RESULT M_Validate(uint8_t *const bytes, const size_t size)
{
    READER reader;
    Reader_OpenBuffer(&reader, bytes, size);
    Reader_Seek(&reader, 8);
    const uint32_t expected = Reader_ReadU32(&reader);
    Reader_Skip(&reader, 2);
    uint8_t key = Reader_ReadU8(&reader);
    for (size_t i = 0xf; i < size; i++) {
        bytes[i] = (uint8_t)(bytes[i] - key);
        key = (uint8_t)(key + 7);
    }
    uint32_t checksum = M_CHECKSUM_START;
    for (size_t i = 0xe; i < size; i++) {
        checksum += bytes[i];
    }
    FAIL_IF(checksum != expected, "replay checksum does not match");
    return OK;
}

static void M_WriteStage(WRITER *const writer, const REPLAY_STAGE *const stage)
{
    Writer_WriteI32(writer, stage->score);
    Writer_WriteU16(writer, stage->rng_seed);
    Writer_WriteU16(writer, stage->point_items_total);
    Writer_WriteU8(writer, stage->power);
    Writer_WriteU8(writer, stage->lives);
    Writer_WriteU8(writer, stage->bombs);
    Writer_WriteU8(writer, stage->rank);
    Writer_WriteU8(writer, stage->power_item_combo);
    Writer_WriteU8(writer, 0);
    Writer_WriteU16(writer, 0);
    for (int32_t i = 0; i < stage->frame_count; i++) {
        Writer_WriteI32(writer, stage->frames[i].frame);
        Writer_WriteU16(writer, stage->frames[i].buttons);
        Writer_WriteU16(writer, 0);
    }
}

static RESULT M_Write(
    WRITER *const writer, const REPLAY_FILE *const file,
    const REPLAY_FORMAT_SAVE *const info, const uint8_t random[4])
{
    uint32_t offsets[8] = {};
    size_t offset = M_HEADER_SIZE;
    for (int32_t i = 1; i <= 7; i++) {
        if (file->stages[i] != nullptr) {
            FAIL_IF(
                file->stages[i]->frame_count < 0
                    || file->stages[i]->frame_count
                        > (INT32_MAX - M_STAGE_HEADER_SIZE) / M_FRAME_SIZE,
                "replay frame count exceeds its size range");
            const size_t stage_size = M_STAGE_HEADER_SIZE
                + (size_t)file->stages[i]->frame_count * M_FRAME_SIZE;
            FAIL_IF(
                offset > UINT32_MAX || stage_size > UINT32_MAX - offset,
                "replay exceeds its offset range");
            offsets[i] = (uint32_t)offset;
            offset += stage_size;
        }
    }
    Writer_WriteBytes(writer, "T6RP", 4);
    Writer_WriteU16(writer, 0x102);
    Writer_WriteU8(writer, file->shot);
    Writer_WriteU8(writer, file->difficulty);
    Writer_WriteU32(writer, 0);
    Writer_WriteU8(writer, random[2]);
    Writer_WriteU8(writer, random[3]);
    Writer_WriteU8(writer, random[0]);
    Writer_WriteU8(writer, random[1]);
    Writer_WriteBytes(writer, info->date, 8);
    Writer_WriteU8(writer, 0);
    Writer_WriteBytes(writer, info->name, sizeof(info->name));
    Writer_WriteU16(writer, 0);
    Writer_WriteU32(writer, file->score);
    Writer_WriteF32(writer, info->slowdown + 1.12f);
    Writer_WriteF32(writer, info->slowdown);
    Writer_WriteF32(writer, info->slowdown + 2.34f);
    for (int32_t i = 1; i <= 7; i++) {
        Writer_WriteU32(writer, offsets[i]);
    }
    for (int32_t i = 1; i <= 7; i++) {
        if (file->stages[i] != nullptr) {
            M_WriteStage(writer, file->stages[i]);
        }
    }
    return OK;
}

RESULT Replay_Read(READER *const reader, REPLAY_FILE **const out)
{
    *out = nullptr;
    const size_t size = Reader_Remaining(reader);
    FAIL_IF(size < M_HEADER_SIZE, "replay header is too short");
    FAIL_IF(size > INT32_MAX, "replay exceeds its size range");
    uint8_t *const bytes = Reader_ReadAlloc(reader, size, &g_HeapAllocator);
    MUST(Reader_Result(reader));
    RESULT result = M_Validate(bytes, size);
    REPLAY_FILE *file = nullptr;
    if (IS_OK(result)) {
        file = Mem_AllocZ(sizeof(REPLAY_FILE));
        READER decoded;
        Reader_OpenBuffer(&decoded, bytes, size);
        result = M_Read(&decoded, file);
    }
    Mem_Free(bytes);
    if (!IS_OK(result)) {
        Replay_Free(file);
        return result;
    }
    *out = file;
    return OK;
}

void Replay_Summarize(const REPLAY_FILE *const file, REPLAY_SUMMARY *const out)
{
    *out = (REPLAY_SUMMARY) {
        .shot = file->shot,
        .difficulty = file->difficulty,
        .score = file->score,
        .slowdown = file->slowdown,
    };
    memcpy(out->date, file->date, sizeof(out->date));
    memcpy(out->name, file->name, sizeof(out->name));
    bool first = true;
    for (int32_t i = 1; i <= 7; i++) {
        const REPLAY_STAGE *const stage = file->stages[i];
        if (stage == nullptr) {
            continue;
        }
        out->stages[i] = true;
        out->stage_scores[i] = stage->score;
        if (first) {
            out->lives = stage->lives;
            out->bombs = stage->bombs;
            first = false;
        }
    }
}

void Replay_Free(REPLAY_FILE *const file)
{
    if (file == nullptr) {
        return;
    }
    for (int32_t i = 1; i <= 7; i++) {
        if (file->stages[i] != nullptr) {
            Mem_Free(file->stages[i]->frames);
            Mem_Free(file->stages[i]);
        }
    }
    Mem_Free(file);
}

RESULT ReplayFormat_Prepare(
    const REPLAY_SAVE_INFO *const info, REPLAY_FORMAT_SAVE *const out)
{
    FAIL_IF(
        info == nullptr || info->name == nullptr || info->date == nullptr,
        "replay name or date is missing");
    FAIL_IF(strlen(info->date) != 8, "replay date must have eight characters");
    for (int32_t i = 0; i < 8; i++) {
        FAIL_IF((uint8_t)info->date[i] > 0x7f, "replay date is not ASCII");
    }
    char *name;
    MUST(Cp932_Encode(info->name, strlen(info->name), &name), "replay name");
    const size_t name_size = strlen(name);
    if (name_size > 8) {
        Mem_Free(name);
        return FAIL("replay name exceeds eight CP932 bytes");
    }
    *out = (REPLAY_FORMAT_SAVE) {};
    memcpy(out->name, name, name_size);
    Mem_Free(name);
    memcpy(out->date, info->date, sizeof(out->date));
    float ratio = (info->achieved_frames / info->expected_frames - 0.5f) * 2.0f;
    if (ratio < 0.0f) {
        ratio = 0.0f;
    } else if (ratio >= 1.0f) {
        ratio = 1.0f;
    }
    out->slowdown = (1.0f - ratio) * 100.0f;
    return OK;
}

RESULT ReplayFormat_Write(
    WRITER *const writer, const REPLAY_FILE *const file,
    const REPLAY_FORMAT_SAVE *const info, const uint8_t random[4])
{
    WRITER buffer;
    Writer_OpenBuffer(&buffer, &g_HeapAllocator);
    RESULT result = M_Write(&buffer, file, info, random);
    if (!IS_OK(result)) {
        Result_Ignore(Writer_Close(&buffer));
        return result;
    }
    const size_t size = Writer_Tell(&buffer);
    uint32_t checksum = M_CHECKSUM_START;
    for (size_t i = 0xe; i < size; i++) {
        checksum += buffer.data[i];
    }
    Writer_Seek(&buffer, 8);
    Writer_WriteU32(&buffer, checksum);
    size_t buffer_size;
    uint8_t *const bytes = Writer_TakeBuffer(&buffer, &buffer_size);
    Result_Ignore(Writer_Close(&buffer));
    uint8_t key = random[0];
    for (size_t i = 0xf; i < size; i++) {
        bytes[i] = (uint8_t)(bytes[i] + key);
        key = (uint8_t)(key + 7);
    }
    Writer_WriteBytes(writer, bytes, buffer_size);
    Mem_Free(bytes);
    return Writer_Result(writer);
}
