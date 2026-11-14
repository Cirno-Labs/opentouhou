#include "th07/sound/bgm.h"

#include <string.h>

#define M_ENTRY_SIZE 0x34
#define M_FORMAT_PCM 1
#define M_BITS_PER_SAMPLE 16

static void M_ReadTrack(READER *const reader, BGM_TRACK *const track)
{
    Reader_ReadBytes(reader, track->name, sizeof(track->name));
    Reader_Read(reader, &track->offset);
    Reader_Read(reader, &track->size);
    Reader_Read(reader, &track->loop_start);
    Reader_Read(reader, &track->loop_end);
    Reader_Read(reader, &track->format_tag);
    Reader_Read(reader, &track->channels);
    Reader_Read(reader, &track->sample_rate);
    Reader_Skip(reader, 4);
    Reader_Read(reader, &track->block_align);
    Reader_Read(reader, &track->bits_per_sample);
    Reader_Skip(reader, 4);
}

RESULT Bgm_ReadList(
    READER *const reader, const ALLOCATOR *const allocator, BGM_LIST *const out)
{
    *out = (BGM_LIST) { .allocator = allocator };
    const size_t start = Reader_Tell(reader);
    int32_t count = 0;
    for (;;) {
        FAIL_IF(
            Reader_Remaining(reader) == 0, "%s has no end entry",
            BGM_LIST_PATH);
        // As the original, only the first byte of the end entry is read.
        uint8_t first;
        Reader_Read(reader, &first);
        if (first == 0) {
            break;
        }
        Reader_Seek(reader, start + (size_t)count * M_ENTRY_SIZE);
        BGM_TRACK track;
        M_ReadTrack(reader, &track);
        MUST(Reader_Result(reader), "%s", BGM_LIST_PATH);
        FAIL_IF(
            memchr(track.name, '\0', sizeof(track.name)) == nullptr,
            "%s: the name of track %d has no end", BGM_LIST_PATH, count);
        count++;
    }
    FAIL_IF(count == 0, "%s has no track", BGM_LIST_PATH);
    out->tracks =
        Allocator_Alloc(allocator, (size_t)count * sizeof(*out->tracks));
    out->count = count;
    Reader_Seek(reader, start);
    for (int32_t i = 0; i < count; i++) {
        M_ReadTrack(reader, &out->tracks[i]);
    }
    return OK;
}

void Bgm_FreeList(BGM_LIST *const list)
{
    if (list->tracks != nullptr) {
        Allocator_Free(
            list->allocator, list->tracks,
            (size_t)list->count * sizeof(*list->tracks));
    }
    *list = (BGM_LIST) {};
}

int32_t Bgm_FindTrack(const BGM_LIST *const list, const char *const path)
{
    const char *name = strrchr(path, '/');
    if (name == nullptr) {
        name = strrchr(path, '\\');
    }
    name = name != nullptr ? name + 1 : path;
    for (int32_t i = 0; i < list->count; i++) {
        if (strcmp(list->tracks[i].name, name) == 0) {
            return i;
        }
    }
    return 0;
}

RESULT Bgm_LoadTrack(
    const BGM_TRACK *const track, READER *const file, AUDIO_STREAM **const out)
{
    *out = nullptr;
    const char *const name = track->name;
    FAIL_IF(
        track->format_tag != M_FORMAT_PCM
            || track->bits_per_sample != M_BITS_PER_SAMPLE
            || track->channels == 0
            || track->block_align != track->channels * 2,
        "track %s of %s is not 16-bit PCM", name, BGM_PATH);
    const int32_t block = track->block_align;
    const int32_t start = track->loop_start > 0 ? track->loop_start : 0;
    FAIL_IF(
        track->loop_end < 0 || track->loop_end % block != 0
            || start % block != 0
            || (track->loop_end != 0 && start >= track->loop_end),
        "track %s of %s has a loop that is not whole sample frames", name,
        BGM_PATH);
    MUST(Reader_Narrow(file, track->offset, (size_t)track->loop_end));
    const AUDIO_PLAY_OPTIONS options = {
        .group = AUDIO_GROUP_MUSIC,
        .volume = 1.0f,
        .pan = 0.0f,
        .pitch = 1.0f,
        .loop = {
            .enabled = track->loop_end != 0,
            .begin = (uint64_t)(start / block),
            .end = (uint64_t)(track->loop_end / block),
        },
    };
    const AUDIO_PCM_FORMAT format = {
        .sample_rate = track->sample_rate,
        .channels = track->channels,
    };
    return AudioStream_LoadPcm(file, format, &options, out);
}
