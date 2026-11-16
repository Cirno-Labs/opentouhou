#include "th01/mdrv_data.h"

#include "core/base/arena.h"
#include "core/base/cp932.h"

#include <string.h>

#define M_RECORD_LIMIT 65536
#define M_WORK_CAPACITY (1024 * 1024)

static RESULT M_ReadRecord(
    READER *const reader, ARENA *const work, READER *const record)
{
    MUST(Reader_Result(reader));
    const size_t size = Reader_Remaining(reader);
    FAIL_IF(size > M_RECORD_LIMIT, "MDRV record exceeds 16-bit offsets");
    void *const bytes = Reader_ReadAlloc(reader, size, Arena_Allocator(work));
    MUST(Reader_Result(reader));
    Reader_OpenBuffer(record, bytes, size, nullptr);
    return OK;
}

static RESULT M_Text(
    READER *const reader, const bool fixed, ARENA *const work,
    const char **const out)
{
    const size_t start = Reader_Tell(reader);
    size_t size = 0;
    const char *bytes;
    if (fixed) {
        bytes = Reader_Borrow(reader, 8);
        MUST(Reader_Result(reader));
        size = 8;
        while (size != 0 && (bytes[size - 1] == 0 || bytes[size - 1] == ' ')) {
            size--;
        }
    } else {
        while (true) {
            const uint8_t byte = Reader_ReadU8(reader);
            MUST(Reader_Result(reader));
            if (byte == '$') {
                break;
            }
            size++;
        }
        READER text = *reader;
        Reader_Seek(&text, start);
        bytes = Reader_Borrow(&text, size);
    }
    char *decoded;
    MUST(Cp932_Decode(bytes, size, &decoded));
    const size_t decoded_size = strlen(decoded) + 1;
    char *const copy = Arena_Alloc(work, decoded_size);
    memcpy(copy, decoded, decoded_size);
    Mem_Free(decoded);
    *out = copy;
    return OK;
}

static bool M_ValidChannel(const uint8_t channel)
{
    return channel == 0 || (channel >= 0x80 && channel <= 0x85)
        || (channel >= 0x40 && channel <= 0x42)
        || (channel >= 0x20 && channel <= 0x23) || channel == 0x10;
}

static RESULT M_ReadMusic(
    READER *const record, ARENA *const work, MDRV_MUSIC *const music)
{
    const uint8_t major = Reader_ReadU8(record);
    const uint8_t minor = Reader_ReadU8(record);
    music->track_count = Reader_ReadU8(record);
    music->flags = Reader_ReadU8(record);
    music->hardware = Reader_ReadU16(record);
    MUST(Reader_Result(record));
    FAIL_IF(major != 2 || minor != 3, "Unsupported MDT version");
    FAIL_IF(
        (size_t)music->track_count * 4 + 6 > Reader_Remaining(record),
        "Truncated MDT directory");
    MDRV_TRACK *const tracks =
        Arena_AllocArr(work, music->track_count, sizeof(*tracks));
    music->tracks = tracks;
    for (int32_t i = 0; i < music->track_count; i++) {
        tracks[i].offset = Reader_ReadU16(record);
        tracks[i].channel = Reader_ReadU8(record);
        tracks[i].reserved = Reader_ReadU8(record);
        FAIL_IF(!M_ValidChannel(tracks[i].channel), "Invalid MDT channel");
    }
    music->fm_offset = Reader_ReadU16(record);
    music->ssg_offset = Reader_ReadU16(record);
    music->metadata_flags = Reader_ReadU8(record);
    music->reserved = Reader_ReadU8(record);
    MUST(Reader_Result(record));
    FAIL_IF(
        music->fm_offset < Reader_Tell(record)
            || music->ssg_offset < music->fm_offset
            || music->ssg_offset > Reader_Size(record),
        "Invalid MDT instrument offsets");
    FAIL_IF(
        (music->ssg_offset - music->fm_offset) % MDRV_FM_INSTRUMENT_SIZE != 0
            || (Reader_Size(record) - music->ssg_offset)
                    % MDRV_SSG_ENVELOPE_SIZE
                != 0,
        "Incomplete MDT instrument bank");
    FAIL_IF(
        (music->metadata_flags & 0x1f) != 0, "Unsupported MDT metadata flags");
    READER metadata =
        Reader_Slice(record, music->fm_offset - Reader_Tell(record));
    if ((music->metadata_flags & 0x80) != 0) {
        MUST(M_Text(&metadata, true, work, &music->pcm_name));
    }
    if ((music->metadata_flags & 0x20) != 0) {
        MUST(M_Text(&metadata, true, work, &music->spm_name));
    }
    if ((music->metadata_flags & 0x40) != 0) {
        MUST(M_Text(&metadata, false, work, &music->title));
    }
    music->commands_offset = music->fm_offset - Reader_Remaining(&metadata);
    for (int32_t i = 0; i < music->track_count; i++) {
        FAIL_IF(
            (tracks[i].channel != 0 || tracks[i].offset != 0)
                && (tracks[i].offset < music->commands_offset
                    || tracks[i].offset >= music->fm_offset),
            "MDT track overlaps metadata or instruments");
    }
    music->fm_count =
        (music->ssg_offset - music->fm_offset) / MDRV_FM_INSTRUMENT_SIZE;
    music->ssg_count =
        (Reader_Size(record) - music->ssg_offset) / MDRV_SSG_ENVELOPE_SIZE;
    return OK;
}

static RESULT M_ReadEffects(
    READER *const record, ARENA *const work, MDRV_EFFECTS *const effects)
{
    const uint8_t major = Reader_ReadU8(record);
    const uint8_t minor = Reader_ReadU8(record);
    effects->effect_count = Reader_ReadU16(record);
    effects->ssg_offset = Reader_ReadU16(record);
    for (int32_t i = 0; i < 5; i++) {
        effects->reserved[i] = Reader_ReadU16(record);
    }
    MUST(Reader_Result(record));
    FAIL_IF(major != 1 || minor != 0, "Unsupported MDE version");
    const size_t table_end = 16 + (size_t)effects->effect_count * 4;
    FAIL_IF(
        table_end > effects->ssg_offset
            || effects->ssg_offset > Reader_Size(record)
            || (Reader_Size(record) - effects->ssg_offset)
                    % MDRV_SSG_ENVELOPE_SIZE
                != 0,
        "Invalid MDE directory or envelope bank");
    MDRV_EFFECT *const entries =
        Arena_AllocArrZ(work, effects->effect_count, sizeof(*entries));
    effects->effects = entries;
    uint8_t *const groups = Arena_AllocZ(work, effects->ssg_offset);
    for (int32_t i = 0; i < effects->effect_count; i++) {
        MDRV_EFFECT *const effect = &entries[i];
        effect->channel_count = Reader_ReadU8(record);
        effect->base_channel = Reader_ReadU8(record);
        effect->block_offset = Reader_ReadU16(record);
        FAIL_IF(
            effect->channel_count < 1
                || effect->channel_count > MDRV_EFFECT_MAX_CHANNELS
                || effect->base_channel < 0x40
                || effect->base_channel + effect->channel_count > 0x43,
            "Invalid MDE SSG channels");
        const size_t start = effect->block_offset;
        const size_t end = start + (size_t)effect->channel_count * 2;
        FAIL_IF(
            start < table_end || end > effects->ssg_offset,
            "Invalid MDE channel pointer group");
        if (groups[start] == (0x80 | effect->channel_count)) {
            continue;
        }
        for (size_t pos = start; pos < end; pos++) {
            FAIL_IF(groups[pos] != 0, "Overlapping MDE pointer groups");
            groups[pos] = pos == start ? 0x80 | effect->channel_count : 0x40;
        }
    }
    for (int32_t i = 0; i < effects->effect_count; i++) {
        MDRV_EFFECT *const effect = &entries[i];
        Reader_Seek(record, effect->block_offset);
        for (int32_t channel = 0; channel < effect->channel_count; channel++) {
            const uint16_t offset = Reader_ReadU16(record);
            FAIL_IF(
                offset < effect->block_offset + effect->channel_count * 2
                    || offset >= effects->ssg_offset || groups[offset] != 0,
                "Invalid MDE command offset");
            effect->stream_offsets[channel] = offset;
        }
    }
    effects->ssg_count =
        (Reader_Size(record) - effects->ssg_offset) / MDRV_SSG_ENVELOPE_SIZE;
    return Reader_Result(record);
}

static size_t M_StringSize(const char *const text)
{
    return text == nullptr ? 0 : strlen(text) + 1;
}

static const char *M_CopyText(char **const next, const char *const text)
{
    if (text == nullptr) {
        return nullptr;
    }
    const size_t size = M_StringSize(text);
    char *const copy = *next;
    memcpy(copy, text, size);
    *next += size;
    return copy;
}

RESULT MdrvData_LoadMusic(
    READER *const reader, const ALLOCATOR *const allocator,
    MDRV_MUSIC *const out)
{
    *out = (MDRV_MUSIC) {};
    ARENA work;
    Arena_Init(&work, "MDT loading", M_WORK_CAPACITY);
    READER record;
    MDRV_MUSIC music = {};
    RESULT result = M_ReadRecord(reader, &work, &record);
    if (IS_OK(result)) {
        result = M_ReadMusic(&record, &work, &music);
    }
    if (IS_OK(result)) {
        const size_t tracks_size = music.track_count * sizeof(*music.tracks);
        music.size = Reader_Size(&record);
        uint8_t *const block = Allocator_Alloc(
            allocator,
            tracks_size + music.size + M_StringSize(music.pcm_name)
                + M_StringSize(music.spm_name) + M_StringSize(music.title));
        memcpy(block, music.tracks, tracks_size);
        music.tracks = (const MDRV_TRACK *)block;
        uint8_t *const bytes = block + tracks_size;
        Reader_Seek(&record, 0);
        Reader_ReadBytes(&record, bytes, music.size);
        music.bytes = bytes;
        char *text = (char *)(bytes + music.size);
        music.pcm_name = M_CopyText(&text, music.pcm_name);
        music.spm_name = M_CopyText(&text, music.spm_name);
        music.title = M_CopyText(&text, music.title);
        *out = music;
    }
    Arena_Free(&work);
    return result;
}

RESULT MdrvData_LoadEffects(
    READER *const reader, const ALLOCATOR *const allocator,
    MDRV_EFFECTS *const out)
{
    *out = (MDRV_EFFECTS) {};
    ARENA work;
    Arena_Init(&work, "MDE loading", M_WORK_CAPACITY);
    READER record;
    MDRV_EFFECTS effects = {};
    RESULT result = M_ReadRecord(reader, &work, &record);
    if (IS_OK(result)) {
        result = M_ReadEffects(&record, &work, &effects);
    }
    if (IS_OK(result)) {
        const size_t entries_size =
            effects.effect_count * sizeof(*effects.effects);
        effects.size = Reader_Size(&record);
        uint8_t *const block =
            Allocator_Alloc(allocator, entries_size + effects.size);
        memcpy(block, effects.effects, entries_size);
        effects.effects = (const MDRV_EFFECT *)block;
        uint8_t *const bytes = block + entries_size;
        Reader_Seek(&record, 0);
        Reader_ReadBytes(&record, bytes, effects.size);
        effects.bytes = bytes;
        *out = effects;
    }
    Arena_Free(&work);
    return result;
}
