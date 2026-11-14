#pragma once

#include "core/audio.h"
#include "core/base/memory.h"
#include "core/base/result.h"
#include "core/io/reader.h"

// Reads th07's music: thbgm.dat, a 16-byte header that th07/files checks
// and then the raw PCM of every track, and bgm/thbgm.fmt, the list of the
// tracks in it.
//
// A track plays its first loop_end bytes and then repeats the bytes from
// loop_start to loop_end. Its size field is not used, and a loop_end of 0
// plays nothing.

#define BGM_NAME_SIZE 16
#define BGM_PATH "thbgm.dat"
#define BGM_LIST_PATH "bgm/thbgm.fmt"

typedef struct {
    // Holds the file name that finds the track, such as "th07_01.wav".
    char name[BGM_NAME_SIZE];
    // Counts bytes from the start of thbgm.dat.
    uint32_t offset;
    uint32_t size;
    // Count bytes from the start of the track.
    int32_t loop_start;
    int32_t loop_end;
    // Holds the track's WAVEFORMATEX.
    uint16_t format_tag;
    uint16_t channels;
    uint32_t sample_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
} BGM_TRACK;

typedef struct {
    BGM_TRACK *tracks;
    int32_t count;
    const ALLOCATOR *allocator;
} BGM_LIST;

// Reads the entries of thbgm.fmt up to the one with an empty name, with
// tracks from allocator. A list without that end entry, or with no track,
// is a failure.
RESULT Bgm_ReadList(READER *reader, const ALLOCATOR *allocator, BGM_LIST *out);

void Bgm_FreeList(BGM_LIST *list);

// Returns the index of the track named as the file name of path, after its
// last '/' or '\', or 0 when no track has that name.
int32_t Bgm_FindTrack(const BGM_LIST *list, const char *path);

// Loads a paused stream of track in AUDIO_GROUP_MUSIC from file, an owned
// reader over thbgm.dat, which it narrows to the track. Takes the reader
// on success. A track that is not 16-bit PCM, or whose loop is not whole
// sample frames inside the file, is a failure.
RESULT Bgm_LoadTrack(const BGM_TRACK *track, READER *file, AUDIO_STREAM **out);
