#include "stg/wave.h"

#include <math.h>
#include <string.h>

// Finds chunk id among the chunks at start. The search stops when the
// sizes of the chunks passed add up to limit exactly. Otherwise limit wraps
// below zero, and the search runs on to the end of the file.
static bool M_FindChunk(
    READER *const reader, const size_t start, uint32_t limit,
    const char *const id)
{
    Reader_Seek(reader, start);
    while (limit != 0) {
        char name[4];
        Reader_ReadBytes(reader, name, sizeof(name));
        const uint32_t chunk_size = Reader_ReadU32(reader);
        if (Reader_Failed(reader)) {
            return false;
        }
        if (memcmp(name, id, sizeof(name)) == 0) {
            return true;
        }
        limit -= chunk_size + 8;
        Reader_Skip(reader, chunk_size);
    }
    return false;
}

RESULT Wave_Check(READER *const reader, const char *const path)
{
    char tag[4];
    Reader_ReadBytes(reader, tag, sizeof(tag));
    FAIL_IF(memcmp(tag, "RIFF", 4) != 0, "%s is not a wave file", path);
    const uint32_t riff_size = Reader_ReadU32(reader);
    Reader_ReadBytes(reader, tag, sizeof(tag));
    FAIL_IF(memcmp(tag, "WAVE", 4) != 0, "%s is not a wave file", path);
    const size_t start = Reader_Tell(reader);
    const uint32_t limit = riff_size - 12;
    FAIL_IF(
        !M_FindChunk(reader, start, limit, "fmt "), "%s has no format chunk",
        path);
    FAIL_IF(
        !M_FindChunk(reader, start, limit, "data"), "%s has no data chunk",
        path);
    MUST(Reader_Result(reader), "%s", path);
    Reader_Seek(reader, 0);
    return OK;
}

float Wave_Gain(const int32_t hundredths_of_db)
{
    return powf(10.0f, (float)hundredths_of_db / 2000.0f);
}
