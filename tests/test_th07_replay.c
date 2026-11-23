#include "th07/replay.h"

#include "test.h"

#include "replay_lzss_literals.h"
#include "stg/lzss.h"

#include "core/base/memory.h"
#include "core/io/writer.h"

#define M_PLAIN_SIZE 0x54
#define M_HEADER_SIZE 0xe8
#define M_STAGE_HEADER_SIZE 0x2c
#define M_MAX_SIZE 0x4000

// Holds how many frames and frame rate bytes each slot gets; 0 frames
// leaves the slot out.
typedef struct {
    uint8_t difficulty;
    int32_t frames[8];
    int32_t fps[8];
} M_SPEC;

typedef struct {
    uint8_t bytes[M_MAX_SIZE];
    size_t size;
} M_IMAGE;

typedef struct {
    uint8_t *bytes;
    uint32_t bit;
} M_BITS;

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

static void M_PutBits(M_BITS *const bits, const uint32_t value, const int count)
{
    for (int i = count - 1; i >= 0; i--) {
        if ((value >> i & 1) != 0) {
            bits->bytes[bits->bit / 8] |= (uint8_t)(0x80 >> (bits->bit % 8));
        }
        bits->bit++;
    }
}

// Fills a decoded replay: the header, then every present slot's stage,
// then every present slot's frame rates, as SaveReplay lays them out.
static void M_Build(M_IMAGE *const image, const M_SPEC *const spec)
{
    uint8_t *const b = image->bytes;
    memset(b, 0, sizeof(image->bytes));
    memcpy(b, "T7RP", 4);
    M_PutU16(b, 4, 0x1100);
    b[0xc] = 0x5a;
    b[0xd] = 0xc3;
    b[0x54] = 0x99;
    b[0x55] = 0x62;
    b[0x56] = 5;
    b[0x57] = spec->difficulty;
    memcpy(b + 0x58, "11/15", 6);
    memcpy(b + 0x5e, "SYNTHETIC", 9);
    M_PutU16(b, 0x6a, 0x100);
    M_PutU32(b, 0x6c, 12345678);
    for (int32_t i = 0; i < 0x38; i++) {
        b[0x70 + i] = (uint8_t)(i * 3 + 1);
    }
    b[0x70 + 0x25] = 0;
    M_PutU32(b, 0xc8, 0x41b8f5c3);
    M_PutU32(b, 0xcc, 0x41a80000);
    M_PutU32(b, 0xd0, 0x41bd1eb8);
    M_PutU32(b, 0xd4, 30);
    M_PutU32(b, 0xd8, 650752);
    M_PutU32(b, 0xdc, 0x1234abcd);
    memcpy(b + 0xe0, "0100b", 6);
    size_t offset = M_HEADER_SIZE;
    for (int32_t slot = 1; slot <= 7; slot++) {
        if (spec->frames[slot] == 0) {
            continue;
        }
        M_PutU32(b, 0x1c + 4 * (size_t)(slot - 1), (uint32_t)offset);
        uint8_t *const s = b + offset;
        M_PutU32(s, 0x00, (uint32_t)slot * 1000000 + 7);
        M_PutU32(s, 0x04, (uint32_t)slot * 100 + 1);
        M_PutU32(s, 0x08, (uint32_t)slot * 1000 + 2);
        M_PutU32(s, 0x0c, (uint32_t)slot * 1000 + 3);
        M_PutU32(s, 0x10, (uint32_t)slot * 1000 + 4);
        M_PutU32(s, 0x14, (uint32_t)slot * 1000 + 5);
        M_PutU32(s, 0x18, (uint32_t)-1);
        M_PutU32(s, 0x1c, (uint32_t)slot * 10 + 6);
        M_PutU16(s, 0x20, (uint16_t)(0xab00 + slot));
        s[0x22] = (uint8_t)(100 + slot);
        s[0x23] = (uint8_t)(2 + slot);
        s[0x24] = (uint8_t)(1 + slot);
        s[0x25] = (uint8_t)(20 + slot);
        s[0x26] = (uint8_t)(30 + slot);
        s[0x27] = (uint8_t)(40 + slot);
        memset(s + 0x28, 0xee, 4);
        for (int32_t i = 0; i < spec->frames[slot]; i++) {
            M_PutU16(
                s, M_STAGE_HEADER_SIZE + 4 * (size_t)i,
                (uint16_t)(slot << 8 | i));
            M_PutU16(
                s, M_STAGE_HEADER_SIZE + 4 * (size_t)i + 2,
                (uint16_t)(0xcd00 + i));
        }
        offset += M_STAGE_HEADER_SIZE + 4 * (size_t)spec->frames[slot];
    }
    for (int32_t slot = 1; slot <= 7; slot++) {
        if (spec->frames[slot] == 0) {
            continue;
        }
        M_PutU32(b, 0x38 + 4 * (size_t)(slot - 1), (uint32_t)offset);
        for (int32_t i = 0; i < spec->fps[slot]; i++) {
            b[offset++] = (uint8_t)(slot * 16 + i);
        }
    }
    image->size = offset;
}

// Packs the data after the plain bytes as LZSS literals, ending without the
// end marker's last partial byte as the original encoder does, and sets
// the sizes. Returns the file before its checksum and cipher.
static M_IMAGE M_Pack(const M_IMAGE *const image)
{
    M_IMAGE file = {};
    memcpy(file.bytes, image->bytes, M_PLAIN_SIZE);
    const size_t packed = ReplayTestLzss_EncodeLiterals(
        image->bytes + M_PLAIN_SIZE, image->size - M_PLAIN_SIZE,
        file.bytes + M_PLAIN_SIZE);
    file.size = M_PLAIN_SIZE + packed;
    M_PutU32(file.bytes, 0x10, (uint32_t)image->size);
    M_PutU32(file.bytes, 0x14, (uint32_t)packed);
    M_PutU32(file.bytes, 0x18, (uint32_t)(image->size - M_PLAIN_SIZE));
    return file;
}

// Sets the checksum and stores every byte from 0x10 on plus the rolling key.
static void M_SealBytes(uint8_t *const bytes, const size_t size)
{
    uint32_t checksum = 0x3f000318;
    for (size_t i = 0xd; i < size; i++) {
        checksum += bytes[i];
    }
    M_PutU32(bytes, 8, checksum);
    uint8_t key = bytes[0xd];
    for (size_t i = 0x10; i < size; i++) {
        bytes[i] = (uint8_t)(bytes[i] + key);
        key = (uint8_t)(key + 7);
    }
}

static void M_Seal(M_IMAGE *const file)
{
    M_SealBytes(file->bytes, file->size);
}

static M_IMAGE M_File(const M_SPEC *const spec)
{
    M_IMAGE image;
    M_Build(&image, spec);
    M_IMAGE file = M_Pack(&image);
    M_Seal(&file);
    return file;
}

static const M_SPEC m_TwoStages = {
    .difficulty = 2,
    .frames = { [1] = 5, [2] = 6 },
    .fps = { [1] = 3, [2] = 2 },
};

static REPLAY_FILE *M_ReadBytes(const uint8_t *const bytes, const size_t size)
{
    READER reader;
    Reader_OpenBuffer(&reader, bytes, size, nullptr);
    REPLAY_FILE *replay = nullptr;
    CHECK(Result_Log(Replay_Read(&reader, &replay), LOG_LEVEL_ERROR));
    Reader_Close(&reader);
    return replay;
}

static void M_RejectBytes(const uint8_t *const bytes, const size_t size)
{
    READER reader;
    Reader_OpenBuffer(&reader, bytes, size, nullptr);
    REPLAY_FILE *replay = nullptr;
    CHECK(!Result_Ignore(Replay_Read(&reader, &replay)));
    CHECK(replay == nullptr);
    Reader_Close(&reader);
    Replay_Free(replay);
}

static REPLAY_FILE *M_Read(const M_IMAGE *const file)
{
    return M_ReadBytes(file->bytes, file->size);
}

static void M_Reject(const M_IMAGE *const file)
{
    M_RejectBytes(file->bytes, file->size);
}

static void M_RejectImage(const M_IMAGE *const image)
{
    M_IMAGE file = M_Pack(image);
    M_Seal(&file);
    M_Reject(&file);
}

TEST(reads_every_header_field)
{
    const M_IMAGE file = M_File(&m_TwoStages);
    REPLAY_FILE *const replay = M_Read(&file);
    if (replay == nullptr) {
        return;
    }
    CHECK_EQ_INT(replay->shot, 5);
    CHECK_EQ_INT(replay->difficulty, 2);
    CHECK_EQ_STR(replay->date, "11/15");
    CHECK_EQ_STR(replay->name, "SYNTHETIC");
    CHECK_EQ_INT(replay->score, 12345678);
    CHECK(replay->slowdown == 21.0f);
    for (int32_t i = 0; i < 0x38; i++) {
        CHECK_EQ_INT(replay->config[i], i == 0x25 ? 0 : i * 3 + 1);
    }
    CHECK_EQ_INT(replay->exe_size, 650752);
    CHECK_EQ_INT(replay->exe_checksum, 0x1234abcd);
    CHECK_EQ_STR(replay->exe_version, "0100b");
    Replay_Free(replay);
}

TEST(reads_every_stage_field_and_every_physical_frame)
{
    const M_IMAGE file = M_File(&m_TwoStages);
    REPLAY_FILE *const replay = M_Read(&file);
    if (replay == nullptr) {
        return;
    }
    CHECK(replay->stages[0] == nullptr);
    CHECK(replay->stages[3] == nullptr);
    for (int32_t slot = 1; slot <= 2; slot++) {
        const REPLAY_STAGE *const stage = replay->stages[slot];
        CHECK(stage != nullptr);
        if (stage == nullptr) {
            continue;
        }
        CHECK_EQ_INT(stage->score, slot * 1000000 + 7);
        CHECK_EQ_INT(stage->point_items_total, slot * 100 + 1);
        CHECK_EQ_INT(stage->cherry, slot * 1000 + 2);
        CHECK_EQ_INT(stage->cherry_max, slot * 1000 + 3);
        CHECK_EQ_INT(stage->cherry_plus, slot * 1000 + 4);
        CHECK_EQ_INT(stage->graze_total, slot * 1000 + 5);
        CHECK_EQ_INT(stage->point_extends, -1);
        CHECK_EQ_INT(stage->next_extend_points, slot * 10 + 6);
        CHECK_EQ_INT(stage->rng_seed, 0xab00 + slot);
        CHECK_EQ_INT(stage->power, 100 + slot);
        CHECK_EQ_INT(stage->lives, 2 + slot);
        CHECK_EQ_INT(stage->bombs, 1 + slot);
        CHECK_EQ_INT(stage->rank, 20 + slot);
        CHECK_EQ_INT(stage->power_item_combo, 30 + slot);
        CHECK_EQ_INT(stage->spell_cards, 40 + slot);
        CHECK_EQ_INT(stage->frame_count, m_TwoStages.frames[slot]);
        for (int32_t i = 0; i < stage->frame_count; i++) {
            CHECK_EQ_INT(stage->frames[i].buttons, slot << 8 | i);
            CHECK_EQ_INT(stage->frames[i].events, 0xcd00 + i);
        }
        CHECK_EQ_INT(replay->fps_counts[slot], m_TwoStages.fps[slot]);
        for (int32_t i = 0; i < replay->fps_counts[slot]; i++) {
            CHECK_EQ_INT(replay->fps[slot][i], slot * 16 + i);
        }
    }
    Replay_Free(replay);
}

TEST(last_stage_ends_at_the_first_frame_rates)
{
    const M_SPEC spec = {
        .difficulty = 0,
        .frames = { [3] = 2, [4] = 3, [5] = 9 },
        .fps = { [3] = 1, [4] = 1, [5] = 4 },
    };
    const M_IMAGE file = M_File(&spec);
    REPLAY_FILE *const replay = M_Read(&file);
    if (replay == nullptr) {
        return;
    }
    CHECK_EQ_INT(replay->stages[3]->frame_count, 2);
    CHECK_EQ_INT(replay->stages[4]->frame_count, 3);
    CHECK_EQ_INT(replay->stages[5]->frame_count, 9);
    CHECK_EQ_INT(replay->fps_counts[3], 1);
    CHECK_EQ_INT(replay->fps_counts[5], 4);
    CHECK_EQ_INT(replay->fps[5][3], 5 * 16 + 3);
    Replay_Free(replay);
}

TEST(phantasm_uses_slot_seven)
{
    const M_SPEC spec = {
        .difficulty = 5,
        .frames = { [7] = 4 },
        .fps = { [7] = 2 },
    };
    const M_IMAGE file = M_File(&spec);
    REPLAY_FILE *const replay = M_Read(&file);
    if (replay == nullptr) {
        return;
    }
    CHECK_EQ_INT(replay->difficulty, 5);
    for (int32_t slot = 0; slot < 7; slot++) {
        CHECK(replay->stages[slot] == nullptr);
    }
    CHECK(replay->stages[7] != nullptr);
    CHECK_EQ_INT(replay->stages[7]->score, 7000007);
    CHECK_EQ_INT(replay->stages[7]->frame_count, 4);
    Replay_Free(replay);
}

TEST(cipher_starts_at_0x10_and_checksum_at_the_key)
{
    // A long stage wraps the 8-bit key many times.
    const M_SPEC spec = {
        .difficulty = 1,
        .frames = { [1] = 600 },
        .fps = { [1] = 21 },
    };
    M_IMAGE image;
    M_Build(&image, &spec);
    M_IMAGE plain = M_Pack(&image);
    plain.bytes[0xe] = 0x11;
    plain.bytes[0xf] = 0x22;
    M_IMAGE file = plain;
    M_Seal(&file);
    CHECK_EQ_INT(file.bytes[0xe], 0x11);
    CHECK_EQ_INT(file.bytes[0xf], 0x22);
    CHECK_EQ_INT(file.bytes[0x10], (uint8_t)(plain.bytes[0x10] + 0xc3));
    CHECK_EQ_INT(file.bytes[0x11], (uint8_t)(plain.bytes[0x11] + 0xca));
    REPLAY_FILE *const replay = M_Read(&file);
    if (replay != nullptr) {
        CHECK_EQ_INT(replay->stages[1]->frame_count, 600);
        CHECK_EQ_INT(replay->stages[1]->frames[599].buttons, 1 << 8 | 599);
    }
    Replay_Free(replay);

    // The checksum covers the key and the padding after it, not the byte
    // before the key.
    M_IMAGE changed = file;
    changed.bytes[0xe]++;
    M_Reject(&changed);
    changed = file;
    changed.bytes[0xd]++;
    M_Reject(&changed);
    changed = file;
    changed.bytes[0xc]++;
    REPLAY_FILE *const kept = M_Read(&changed);
    CHECK(kept != nullptr);
    Replay_Free(kept);
}

TEST(rejects_bad_magic_version_and_checksum)
{
    const M_IMAGE file = M_File(&m_TwoStages);
    M_IMAGE changed = file;
    changed.bytes[3] = 'Q';
    M_Reject(&changed);
    changed = file;
    M_PutU16(changed.bytes, 4, 0x1101);
    M_Reject(&changed);
    changed = file;
    M_PutU32(changed.bytes, 8, M_U32(file.bytes, 8) + 1);
    M_Reject(&changed);
    changed = file;
    changed.bytes[file.size - 1] ^= 0x40;
    M_Reject(&changed);
}

TEST(rejects_short_and_inconsistent_sizes)
{
    const M_IMAGE file = M_File(&m_TwoStages);
    M_IMAGE changed = file;
    changed.size = M_PLAIN_SIZE - 1;
    M_Reject(&changed);

    M_IMAGE image;
    M_Build(&image, &m_TwoStages);
    const M_IMAGE plain = M_Pack(&image);
    // Truncated file: the compressed size no longer matches.
    changed = plain;
    changed.size--;
    M_Seal(&changed);
    M_Reject(&changed);
    const struct {
        size_t offset;
        uint32_t delta;
    } fields[] = {
        { 0x10, 1 }, { 0x14, 1 }, { 0x18, 1 }, { 0x14, 0xffffffff }
    };
    for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); i++) {
        changed = plain;
        M_PutU32(
            changed.bytes, fields[i].offset,
            M_U32(plain.bytes, fields[i].offset) + fields[i].delta);
        M_Seal(&changed);
        M_Reject(&changed);
    }
    // A data size too small for the header, and one past what SaveReplay
    // can write, even with a matching total.
    const uint32_t data_sizes[] = { 0x93, 0x100001 };
    for (size_t i = 0; i < 2; i++) {
        changed = plain;
        M_PutU32(changed.bytes, 0x18, data_sizes[i]);
        M_PutU32(changed.bytes, 0x10, data_sizes[i] + M_PLAIN_SIZE);
        M_Seal(&changed);
        M_Reject(&changed);
    }
}

// Packs a replay of data_size bytes after the plain ones, one stage of empty
// frames and four frame rate bytes, into *size bytes.
static uint8_t *M_LargeFile(const uint32_t data_size, size_t *const size)
{
    const size_t total = M_PLAIN_SIZE + data_size;
    uint8_t *const image = Mem_AllocZ(total);
    memcpy(image, "T7RP", 4);
    M_PutU16(image, 4, 0x1100);
    M_PutU32(image, 0x10, (uint32_t)total);
    M_PutU32(image, 0x18, data_size);
    M_PutU32(image, 0x1c, M_HEADER_SIZE);
    M_PutU32(image, 0x38, (uint32_t)total - 4);
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator, nullptr);
    Writer_WriteBytes(&writer, image, M_PLAIN_SIZE);
    Lzss_Encode(image + M_PLAIN_SIZE, data_size, &writer);
    uint8_t *const file = Writer_TakeBuffer(&writer, size);
    CHECK(Result_Log(Writer_Close(&writer), LOG_LEVEL_ERROR));
    Mem_Free(image);
    M_PutU32(file, 0x14, (uint32_t)(*size - M_PLAIN_SIZE));
    M_SealBytes(file, *size);
    return file;
}

TEST(reads_data_up_to_what_save_replay_can_write)
{
    size_t size;
    uint8_t *file = M_LargeFile(0x100000, &size);
    REPLAY_FILE *const replay = M_ReadBytes(file, size);
    if (replay != nullptr) {
        CHECK_EQ_INT(
            replay->stages[1]->frame_count,
            (0x100000 - (M_HEADER_SIZE - M_PLAIN_SIZE) - M_STAGE_HEADER_SIZE
             - 4)
                / 4);
    }
    Replay_Free(replay);
    Mem_Free(file);
    file = M_LargeFile(0x100004, &size);
    M_RejectBytes(file, size);
    Mem_Free(file);
}

TEST(rejects_lzss_streams_that_end_early_or_overrun)
{
    M_IMAGE image;
    M_Build(&image, &m_TwoStages);
    // Ends one byte early.
    M_IMAGE shorter = image;
    shorter.size--;
    M_IMAGE changed = M_Pack(&shorter);
    M_PutU32(changed.bytes, 0x10, (uint32_t)image.size);
    M_PutU32(changed.bytes, 0x18, (uint32_t)(image.size - M_PLAIN_SIZE));
    M_Seal(&changed);
    M_Reject(&changed);

    // All literals but the last byte, then a three-byte match.
    changed = (M_IMAGE) {};
    memcpy(changed.bytes, image.bytes, M_PLAIN_SIZE);
    M_BITS bits = { .bytes = changed.bytes + M_PLAIN_SIZE };
    for (size_t i = M_PLAIN_SIZE; i < image.size - 1; i++) {
        M_PutBits(&bits, 1, 1);
        M_PutBits(&bits, image.bytes[i], 8);
    }
    M_PutBits(&bits, 0, 1);
    M_PutBits(&bits, 1, 13);
    M_PutBits(&bits, 0, 4);
    M_PutBits(&bits, 0, 14);
    const size_t packed = (bits.bit + 7) / 8;
    changed.size = M_PLAIN_SIZE + packed;
    M_PutU32(changed.bytes, 0x10, (uint32_t)image.size);
    M_PutU32(changed.bytes, 0x14, (uint32_t)packed);
    M_PutU32(changed.bytes, 0x18, (uint32_t)(image.size - M_PLAIN_SIZE));
    M_Seal(&changed);
    M_Reject(&changed);
}

TEST(reads_data_packed_with_matches)
{
    const M_SPEC spec = {
        .difficulty = 3,
        .frames = { [6] = 300 },
        .fps = { [6] = 11 },
    };
    M_IMAGE image;
    M_Build(&image, &spec);
    // Held buttons and empty events repeat, as in a real recording.
    const size_t frames =
        M_U32(image.bytes, 0x1c + 4 * 5) + M_STAGE_HEADER_SIZE;
    for (size_t i = 0; i < 300; i++) {
        M_PutU16(image.bytes, frames + 4 * i, (uint16_t)(i / 40));
        M_PutU16(image.bytes, frames + 4 * i + 2, i == 257 ? 2 : 0);
    }
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator, nullptr);
    Lzss_Encode(image.bytes + M_PLAIN_SIZE, image.size - M_PLAIN_SIZE, &writer);
    size_t packed;
    uint8_t *const data = Writer_TakeBuffer(&writer, &packed);
    CHECK(Result_Log(Writer_Close(&writer), LOG_LEVEL_ERROR));
    CHECK(packed < (image.size - M_PLAIN_SIZE) / 2);
    M_IMAGE file = {};
    memcpy(file.bytes, image.bytes, M_PLAIN_SIZE);
    memcpy(file.bytes + M_PLAIN_SIZE, data, packed);
    Mem_Free(data);
    file.size = M_PLAIN_SIZE + packed;
    M_PutU32(file.bytes, 0x10, (uint32_t)image.size);
    M_PutU32(file.bytes, 0x14, (uint32_t)packed);
    M_PutU32(file.bytes, 0x18, (uint32_t)(image.size - M_PLAIN_SIZE));
    M_Seal(&file);
    REPLAY_FILE *const replay = M_Read(&file);
    if (replay == nullptr) {
        return;
    }
    CHECK_EQ_INT(replay->stages[6]->frame_count, 300);
    CHECK_EQ_INT(replay->stages[6]->frames[257].buttons, 6);
    CHECK_EQ_INT(replay->stages[6]->frames[257].events, 2);
    CHECK_EQ_INT(replay->stages[6]->frames[258].events, 0);
    CHECK_EQ_INT(replay->fps[6][10], 6 * 16 + 10);
    Replay_Free(replay);
}

TEST(rejects_bad_slot_offsets_and_stage_sizes)
{
    M_IMAGE image;
    M_Build(&image, &m_TwoStages);
    M_IMAGE changed = image;
    M_PutU32(changed.bytes, 0x1c, M_HEADER_SIZE - 4);
    M_RejectImage(&changed);
    changed = image;
    M_PutU32(changed.bytes, 0x1c, (uint32_t)image.size);
    M_RejectImage(&changed);
    // Frame rates outside the data, and a stage without them. With four
    // bytes of frame rates, the stage's frames would stay whole.
    const M_SPEC aligned = {
        .difficulty = 1,
        .frames = { [1] = 5 },
        .fps = { [1] = 4 },
    };
    M_IMAGE single;
    M_Build(&single, &aligned);
    changed = single;
    M_PutU32(changed.bytes, 0x38, (uint32_t)single.size);
    M_RejectImage(&changed);
    changed = single;
    M_PutU32(changed.bytes, 0x38, 0);
    M_RejectImage(&changed);
    // Frame rates without a stage.
    changed = image;
    M_PutU32(changed.bytes, 0x38 + 4 * 4, M_U32(image.bytes, 0x38));
    M_RejectImage(&changed);
    // A stage with a partial frame, and one without any frame.
    changed = image;
    M_PutU32(changed.bytes, 0x20, M_U32(image.bytes, 0x20) - 2);
    M_RejectImage(&changed);
    changed = image;
    M_PutU32(changed.bytes, 0x20, M_U32(image.bytes, 0x1c) + 0x2c);
    M_RejectImage(&changed);
    const M_SPEC none = { .difficulty = 1 };
    M_Build(&changed, &none);
    M_RejectImage(&changed);
}

TEST(rejects_bad_shots_difficulties_dates_and_disabled_scores)
{
    M_IMAGE image;
    M_Build(&image, &m_TwoStages);
    M_IMAGE changed = image;
    changed.bytes[0x56] = 6;
    M_RejectImage(&changed);
    changed = image;
    changed.bytes[0x57] = 6;
    M_RejectImage(&changed);
    changed = image;
    changed.bytes[0x59] = 0x81;
    M_RejectImage(&changed);
    changed = image;
    changed.bytes[0x70 + 0x25] = 1;
    M_RejectImage(&changed);
}

TEST(keeps_a_mismatching_executable_identity)
{
    M_IMAGE image;
    M_Build(&image, &m_TwoStages);
    M_PutU32(image.bytes, 0xd8, 1);
    M_PutU32(image.bytes, 0xdc, 2);
    memcpy(image.bytes + 0xe0, "0100a", 6);
    M_IMAGE file = M_Pack(&image);
    M_Seal(&file);
    REPLAY_FILE *const replay = M_Read(&file);
    if (replay == nullptr) {
        return;
    }
    CHECK_EQ_INT(replay->exe_size, 1);
    CHECK_EQ_INT(replay->exe_checksum, 2);
    CHECK_EQ_STR(replay->exe_version, "0100a");
    Replay_Free(replay);
}

TEST(decodes_twelve_byte_cp932_names)
{
    M_IMAGE image;
    M_Build(&image, &m_TwoStages);
    // 魂魄妖夢 in CP932, then four more bytes, filling all twelve.
    const uint8_t name[12] = {
        0x8d, 0xb0, 0xe9, 0xae, 0x97, 0x64, 0x96, 0xb2, 'A', 'B', 'C', 'D',
    };
    memcpy(image.bytes + 0x5e, name, sizeof(name));
    M_IMAGE file = M_Pack(&image);
    M_Seal(&file);
    REPLAY_FILE *const replay = M_Read(&file);
    if (replay == nullptr) {
        return;
    }
    CHECK_EQ_STR(replay->name, "魂魄妖夢ABCD");
    Replay_Free(replay);
}

TEST(write_then_read_round_trips_every_field)
{
    REPLAY_FRAME frames1[3] = {
        { .buttons = 0x11, .events = 0x22 },
        { .buttons = 0x33, .events = 0x44 },
        { .buttons = 0x55, .events = 0x66 },
    };
    REPLAY_STAGE stage1 = {
        .score = 123456,
        .rng_seed = 0xabcd,
        .point_items_total = 7,
        .power = 8,
        .lives = 2,
        .bombs = 3,
        .rank = 4,
        .power_item_combo = 5,
        .cherry = 10,
        .cherry_max = 20,
        .cherry_plus = 30,
        .graze_total = 40,
        .spell_cards = 6,
        .point_extends = -1,
        .next_extend_points = 50,
        .frames = frames1,
        .frame_count = 3,
    };
    REPLAY_FRAME frames7[2] = {
        { .buttons = 0x77, .events = 0x88 },
        { .buttons = 0x99, .events = 0xaa },
    };
    REPLAY_STAGE stage7 = { .score = 654321,
                            .frames = frames7,
                            .frame_count = 2 };
    uint8_t fps1[2] = { 60, 61 };
    uint8_t fps7[3] = { 30, 31, 32 };
    REPLAY_FILE file = {
        .shot = 5,
        .difficulty = 3,
        .score = 999999,
        .slowdown = 12.5f,
        .stages = { [1] = &stage1, [7] = &stage7 },
        .fps = { [1] = fps1, [7] = fps7 },
        .fps_counts = { [1] = 2, [7] = 3 },
        .exe_size = 650752,
        .exe_checksum = 0x1234abcd,
    };
    memcpy(file.date, "11/15", 6);
    memcpy(file.name, "魂魄妖夢", strlen("魂魄妖夢") + 1);
    for (int32_t i = 0; i < 0x38; i++) {
        file.config[i] = (uint8_t)(i * 3 + 1);
    }
    file.config[0x25] = 0;
    memcpy(file.exe_version, "0100b", 6);

    const uint8_t random[3] = { 0xc3, 0x5a, 0x11 };
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator, nullptr);
    CHECK(Result_Log(Replay_Write(&file, &writer, random), LOG_LEVEL_ERROR));
    size_t size;
    uint8_t *const bytes = Writer_TakeBuffer(&writer, &size);
    CHECK(Result_Log(Writer_Close(&writer), LOG_LEVEL_ERROR));

    REPLAY_FILE *const replay = M_ReadBytes(bytes, size);
    Mem_Free(bytes);
    if (replay == nullptr) {
        return;
    }
    CHECK_EQ_INT(replay->shot, file.shot);
    CHECK_EQ_INT(replay->difficulty, file.difficulty);
    CHECK_EQ_STR(replay->date, file.date);
    CHECK_EQ_STR(replay->name, file.name);
    CHECK_EQ_INT(replay->score, file.score);
    CHECK(replay->slowdown == file.slowdown);
    for (int32_t i = 0; i < 0x38; i++) {
        CHECK_EQ_INT(replay->config[i], file.config[i]);
    }
    CHECK_EQ_INT(replay->exe_size, file.exe_size);
    CHECK_EQ_INT(replay->exe_checksum, file.exe_checksum);
    CHECK_EQ_STR(replay->exe_version, file.exe_version);
    for (int32_t slot = 0; slot < 8; slot++) {
        CHECK(
            (replay->stages[slot] == nullptr)
            == (file.stages[slot] == nullptr));
    }
    CHECK_EQ_INT(replay->stages[1]->score, stage1.score);
    CHECK_EQ_INT(replay->stages[1]->rng_seed, stage1.rng_seed);
    CHECK_EQ_INT(
        replay->stages[1]->point_items_total, stage1.point_items_total);
    CHECK_EQ_INT(replay->stages[1]->power, stage1.power);
    CHECK_EQ_INT(replay->stages[1]->lives, stage1.lives);
    CHECK_EQ_INT(replay->stages[1]->bombs, stage1.bombs);
    CHECK_EQ_INT(replay->stages[1]->rank, stage1.rank);
    CHECK_EQ_INT(replay->stages[1]->power_item_combo, stage1.power_item_combo);
    CHECK_EQ_INT(replay->stages[1]->cherry, stage1.cherry);
    CHECK_EQ_INT(replay->stages[1]->cherry_max, stage1.cherry_max);
    CHECK_EQ_INT(replay->stages[1]->cherry_plus, stage1.cherry_plus);
    CHECK_EQ_INT(replay->stages[1]->graze_total, stage1.graze_total);
    CHECK_EQ_INT(replay->stages[1]->spell_cards, stage1.spell_cards);
    CHECK_EQ_INT(replay->stages[1]->point_extends, stage1.point_extends);
    CHECK_EQ_INT(
        replay->stages[1]->next_extend_points, stage1.next_extend_points);
    CHECK_EQ_INT(replay->stages[1]->frame_count, stage1.frame_count);
    for (int32_t i = 0; i < stage1.frame_count; i++) {
        CHECK_EQ_INT(replay->stages[1]->frames[i].buttons, frames1[i].buttons);
        CHECK_EQ_INT(replay->stages[1]->frames[i].events, frames1[i].events);
    }
    CHECK_EQ_INT(replay->fps_counts[1], 2);
    CHECK_EQ_INT(replay->fps[1][0], fps1[0]);
    CHECK_EQ_INT(replay->fps[1][1], fps1[1]);

    CHECK_EQ_INT(replay->stages[7]->score, stage7.score);
    CHECK_EQ_INT(replay->stages[7]->frame_count, stage7.frame_count);
    for (int32_t i = 0; i < stage7.frame_count; i++) {
        CHECK_EQ_INT(replay->stages[7]->frames[i].buttons, frames7[i].buttons);
        CHECK_EQ_INT(replay->stages[7]->frames[i].events, frames7[i].events);
    }
    CHECK_EQ_INT(replay->fps_counts[7], 3);
    for (int32_t i = 0; i < 3; i++) {
        CHECK_EQ_INT(replay->fps[7][i], fps7[i]);
    }
    Replay_Free(replay);
}

TEST(summary_holds_what_the_replay_menu_shows)
{
    REPLAY_STAGE second = { .score = 100, .lives = 4 };
    REPLAY_STAGE seventh = { .score = 700 };
    REPLAY_FILE file = {
        .shot = 3,
        .difficulty = 4,
        .score = 800,
        .slowdown = 1.5f,
        .stages = { [2] = &second, [7] = &seventh },
    };
    memcpy(file.date, "01/02", 6);
    memcpy(file.name, "NAME", 5);
    REPLAY_SUMMARY summary;
    Replay_Summarize(&file, &summary);
    CHECK_EQ_INT(summary.shot, 3);
    CHECK_EQ_INT(summary.difficulty, 4);
    CHECK_EQ_INT(summary.score, 800);
    CHECK(summary.slowdown == 1.5f);
    CHECK_EQ_STR(summary.date, "01/02");
    CHECK_EQ_STR(summary.name, "NAME");
    for (int32_t i = 0; i < 8; i++) {
        CHECK(summary.stages[i] == (i == 2 || i == 7));
    }
    CHECK_EQ_INT(summary.stage_scores[2], 100);
    CHECK_EQ_INT(summary.stage_scores[7], 700);
    CHECK_EQ_INT(summary.stage_scores[3], 0);
}
