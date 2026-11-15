#include "th07/replay/file.h"

#include "stg/lzss.h"

#include "core/base/cp932.h"
#include "core/base/memory.h"
#include "core/io/reader.h"

#include <limits.h>
#include <string.h>

#define M_PLAIN_SIZE 0x54
#define M_KEY_OFFSET 0xd
#define M_CIPHER_START 0x10
#define M_HEADER_SIZE 0xe8
#define M_STAGE_HEADER_SIZE 0x2c
#define M_FRAME_SIZE 4
#define M_VERSION 0x1100
#define M_CHECKSUM_START 0x3f000318u
#define M_CONFIG_SCORE_DISABLED 0x25
// SaveReplay packs the data from a buffer of 1 MiB.
#define M_MAX_DATA_SIZE 0x100000

typedef struct {
    uint32_t size;
    uint32_t compressed_size;
    uint32_t data_size;
    uint32_t stages[7];
    uint32_t fps[7];
} M_LAYOUT;

static RESULT M_Decrypt(uint8_t *const bytes, const size_t size)
{
    FAIL_IF(memcmp(bytes, "T7RP", 4) != 0, "invalid replay magic");
    const uint16_t version = (uint16_t)(bytes[4] | bytes[5] << 8);
    FAIL_IF(version != M_VERSION, "unsupported replay version 0x%x", version);
    uint8_t key = bytes[M_KEY_OFFSET];
    for (size_t i = M_CIPHER_START; i < size; i++) {
        bytes[i] = (uint8_t)(bytes[i] - key);
        key = (uint8_t)(key + 7);
    }
    uint32_t checksum = M_CHECKSUM_START;
    for (size_t i = M_KEY_OFFSET; i < size; i++) {
        checksum += bytes[i];
    }
    const uint32_t expected = (uint32_t)bytes[8] | (uint32_t)bytes[9] << 8
        | (uint32_t)bytes[10] << 16 | (uint32_t)bytes[11] << 24;
    FAIL_IF(checksum != expected, "replay checksum does not match");
    return OK;
}

static RESULT M_ReadLayout(
    const uint8_t *const bytes, const size_t size, M_LAYOUT *const out)
{
    READER reader;
    Reader_OpenBuffer(&reader, bytes, M_PLAIN_SIZE, nullptr);
    Reader_Seek(&reader, 0x10);
    out->size = Reader_ReadU32(&reader);
    out->compressed_size = Reader_ReadU32(&reader);
    out->data_size = Reader_ReadU32(&reader);
    for (int32_t i = 0; i < 7; i++) {
        out->stages[i] = Reader_ReadU32(&reader);
    }
    for (int32_t i = 0; i < 7; i++) {
        out->fps[i] = Reader_ReadU32(&reader);
    }
    const RESULT result = Reader_Result(&reader);
    Reader_Close(&reader);
    MUST(result);
    FAIL_IF(
        out->compressed_size != size - M_PLAIN_SIZE,
        "replay compressed size does not match the file");
    FAIL_IF(
        out->data_size < M_HEADER_SIZE - M_PLAIN_SIZE
            || out->data_size > M_MAX_DATA_SIZE
            || out->size != out->data_size + M_PLAIN_SIZE,
        "replay data size is invalid");
    return OK;
}

static RESULT M_Decompress(
    const uint8_t *const bytes, const M_LAYOUT *const layout,
    uint8_t *const out)
{
    memcpy(out, bytes, M_PLAIN_SIZE);
    READER packed;
    Reader_OpenBuffer(
        &packed, bytes + M_PLAIN_SIZE, layout->compressed_size, nullptr);
    size_t decoded;
    const RESULT result = Lzss_Decode(
        &packed, out + M_PLAIN_SIZE, layout->data_size, &decoded, nullptr);
    Reader_Close(&packed);
    MUST(result, "replay data");
    FAIL_IF(decoded != layout->data_size, "replay data is truncated");
    return OK;
}

// Returns where the region at offset ends: at the next stage or frame rate
// region, or at the end of the data. SaveReplay writes every stage, then
// every frame rate region, so the last stage ends at the first frame rates.
static size_t M_RegionEnd(const M_LAYOUT *const layout, const uint32_t offset)
{
    size_t end = layout->size;
    for (int32_t i = 0; i < 7; i++) {
        if (layout->stages[i] > offset && layout->stages[i] < end) {
            end = layout->stages[i];
        }
        if (layout->fps[i] > offset && layout->fps[i] < end) {
            end = layout->fps[i];
        }
    }
    return end;
}

static RESULT M_ReadStage(READER *const reader, REPLAY_STAGE *const stage)
{
    FAIL_IF(
        Reader_Remaining(reader) < M_STAGE_HEADER_SIZE + M_FRAME_SIZE,
        "replay stage is too short");
    FAIL_IF(
        (Reader_Remaining(reader) - M_STAGE_HEADER_SIZE) % M_FRAME_SIZE != 0,
        "replay stage has a partial frame");
    stage->score = Reader_ReadI32(reader);
    stage->point_items_total = Reader_ReadI32(reader);
    stage->cherry = Reader_ReadI32(reader);
    stage->cherry_max = Reader_ReadI32(reader);
    stage->cherry_plus = Reader_ReadI32(reader);
    stage->graze_total = Reader_ReadI32(reader);
    stage->point_extends = Reader_ReadI32(reader);
    stage->next_extend_points = Reader_ReadI32(reader);
    stage->rng_seed = Reader_ReadU16(reader);
    stage->power = Reader_ReadU8(reader);
    stage->lives = Reader_ReadU8(reader);
    stage->bombs = Reader_ReadU8(reader);
    stage->rank = Reader_ReadU8(reader);
    stage->power_item_combo = Reader_ReadU8(reader);
    stage->spell_cards = Reader_ReadU8(reader);
    Reader_Skip(reader, 4);
    stage->frame_count = (int32_t)(Reader_Remaining(reader) / M_FRAME_SIZE);
    stage->frames =
        Mem_AllocArr((size_t)stage->frame_count, sizeof(REPLAY_FRAME));
    for (int32_t i = 0; i < stage->frame_count; i++) {
        stage->frames[i].buttons = Reader_ReadU16(reader);
        stage->frames[i].events = Reader_ReadU16(reader);
    }
    return Reader_Result(reader);
}

static RESULT M_ReadSlot(
    READER *const reader, const M_LAYOUT *const layout, const int32_t slot,
    REPLAY_FILE *const file)
{
    const uint32_t stage = layout->stages[slot - 1];
    const uint32_t fps = layout->fps[slot - 1];
    FAIL_IF(
        stage < M_HEADER_SIZE || stage >= layout->size || fps < M_HEADER_SIZE
            || fps >= layout->size,
        "replay slot %d is outside the data", slot);
    Reader_Seek(reader, stage);
    READER stage_reader =
        Reader_Slice(reader, M_RegionEnd(layout, stage) - stage);
    file->stages[slot] = Mem_AllocZ(sizeof(REPLAY_STAGE));
    MUST(
        M_ReadStage(&stage_reader, file->stages[slot]), "replay slot %d", slot);
    const size_t fps_count = M_RegionEnd(layout, fps) - fps;
    Reader_Seek(reader, fps);
    file->fps[slot] = Reader_ReadAlloc(reader, fps_count, &g_HeapAllocator);
    file->fps_counts[slot] = (int32_t)fps_count;
    return Reader_Result(reader);
}

static RESULT M_Read(
    READER *const reader, const M_LAYOUT *const layout, REPLAY_FILE *const file)
{
    Reader_Seek(reader, 0x56);
    file->shot = Reader_ReadU8(reader);
    file->difficulty = Reader_ReadU8(reader);
    FAIL_IF(file->shot > 5, "invalid replay shot %u", file->shot);
    FAIL_IF(
        file->difficulty > 5, "invalid replay difficulty %u", file->difficulty);
    Reader_ReadBytes(reader, file->date, sizeof(file->date));
    file->date[sizeof(file->date) - 1] = '\0';
    for (size_t i = 0; file->date[i] != '\0'; i++) {
        FAIL_IF((uint8_t)file->date[i] > 0x7f, "replay date is not ASCII");
    }
    char name_bytes[12];
    Reader_ReadBytes(reader, name_bytes, sizeof(name_bytes));
    size_t name_size = 0;
    while (name_size < sizeof(name_bytes) && name_bytes[name_size] != '\0') {
        name_size++;
    }
    char *name;
    MUST(Cp932_Decode(name_bytes, name_size, &name), "replay name");
    memcpy(file->name, name, strlen(name) + 1);
    Mem_Free(name);
    Reader_Skip(reader, 2);
    file->score = Reader_ReadU32(reader);
    Reader_ReadBytes(reader, file->config, sizeof(file->config));
    FAIL_IF(
        file->config[M_CONFIG_SCORE_DISABLED] != 0,
        "replay was recorded with scores disabled");
    Reader_Skip(reader, 0x24);
    file->slowdown = Reader_ReadF32(reader);
    Reader_Skip(reader, 8);
    file->exe_size = Reader_ReadU32(reader);
    file->exe_checksum = Reader_ReadU32(reader);
    Reader_ReadBytes(reader, file->exe_version, sizeof(file->exe_version));
    file->exe_version[sizeof(file->exe_version) - 1] = '\0';
    MUST(Reader_Result(reader));
    bool any_stage = false;
    for (int32_t slot = 1; slot <= 7; slot++) {
        if (layout->stages[slot - 1] != 0 || layout->fps[slot - 1] != 0) {
            any_stage = true;
            MUST(M_ReadSlot(reader, layout, slot, file));
        }
    }
    FAIL_IF(!any_stage, "replay has no stages");
    return OK;
}

static RESULT M_Decode(
    uint8_t *const bytes, const size_t size, const char *const name,
    REPLAY_FILE *const file)
{
    MUST(M_Decrypt(bytes, size));
    M_LAYOUT layout;
    MUST(M_ReadLayout(bytes, size, &layout));
    uint8_t *const data = Mem_Alloc(layout.size);
    RESULT result = M_Decompress(bytes, &layout, data);
    if (IS_OK(result)) {
        READER reader;
        Reader_OpenBuffer(&reader, data, layout.size, name);
        result = M_Read(&reader, &layout, file);
        Reader_Close(&reader);
    }
    Mem_Free(data);
    return result;
}

RESULT Replay_Read(READER *const reader, REPLAY_FILE **const out)
{
    *out = nullptr;
    const size_t size = Reader_Remaining(reader);
    FAIL_IF(size < M_PLAIN_SIZE, "replay header is too short");
    FAIL_IF(size > INT32_MAX, "replay exceeds its size range");
    uint8_t *const bytes = Reader_ReadAlloc(reader, size, &g_HeapAllocator);
    MUST(Reader_Result(reader));
    REPLAY_FILE *const file = Mem_AllocZ(sizeof(REPLAY_FILE));
    const RESULT result = M_Decode(bytes, size, Reader_GetName(reader), file);
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
    for (int32_t i = 1; i <= 7; i++) {
        if (file->stages[i] != nullptr) {
            out->stages[i] = true;
            out->stage_scores[i] = file->stages[i]->score;
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
        Mem_Free(file->fps[i]);
    }
    Mem_Free(file);
}
