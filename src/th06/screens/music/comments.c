// SPDX-License-Identifier: GPL-3.0-only

#include "th06/screens/music/comments.h"

#include "core/base/cp932.h"
#include "core/base/log.h"
#include "core/base/memory.h"

#include <string.h>

#define M_ROW_SIZE 0x42

// Holds a tune as the file stores it, in CP932.
typedef struct {
    uint8_t path[MUSIC_PATH_SIZE];
    uint8_t title[MUSIC_TITLE_BYTES];
    uint8_t comment[MUSIC_COMMENT_ROWS][M_ROW_SIZE];
} M_RAW_TUNE;

typedef struct {
    const uint8_t *data;
    size_t size;
    size_t position;
} M_TEXT;

static uint8_t M_Current(const M_TEXT *const text)
{
    return text->data[text->position];
}

static bool M_IsBreak(const uint8_t c)
{
    return c == '\n' || c == '\r';
}

// Copies the bytes up to the line break into out, at most size - 1 of
// them. Returns false when the data ends first.
static bool M_CopyLine(
    M_TEXT *const text, uint8_t *const out, const size_t size)
{
    size_t length = 0;
    while (!M_IsBreak(M_Current(text))) {
        if (length < size - 1) {
            out[length++] = M_Current(text);
        }
        text->position++;
        if (text->position >= text->size) {
            return false;
        }
    }
    return true;
}

// Skips line breaks. Returns false when the data ends first.
static bool M_SkipBreaks(M_TEXT *const text)
{
    while (M_IsBreak(M_Current(text))) {
        text->position++;
        if (text->position >= text->size) {
            return false;
        }
    }
    return true;
}

// Reads the tune after its '@'. Returns false when the data ends.
static bool M_ReadTune(M_TEXT *const text, M_RAW_TUNE *const tune)
{
    if (!M_CopyLine(text, tune->path, sizeof(tune->path)) || !M_SkipBreaks(text)
        || !M_CopyLine(text, tune->title, sizeof(tune->title))) {
        return false;
    }
    for (int32_t row = 0; row < MUSIC_COMMENT_ROWS; row++) {
        if (M_Current(text) == '@') {
            break;
        }
        if (!M_CopyLine(text, tune->comment[row], M_ROW_SIZE)
            || !M_SkipBreaks(text)) {
            return false;
        }
    }
    return true;
}

static void M_Decode(
    const uint8_t *const bytes, const size_t size, char *const out,
    const char *const what)
{
    size_t length = 0;
    while (length < size && bytes[length] != 0) {
        length++;
    }
    char *utf8;
    if (Result_Ignore(Cp932_DecodeText((const char *)bytes, length, &utf8))) {
        memcpy(out, utf8, strlen(utf8) + 1);
        Mem_Free(utf8);
    } else {
        LOG_WARNING("Dropped the music room %s that does not convert", what);
        out[0] = '\0';
    }
}

static void M_Convert(const M_RAW_TUNE *const raw, MUSIC_TUNE *const out)
{
    *out = (MUSIC_TUNE) {};
    memcpy(out->path, raw->path, sizeof(out->path));
    M_Decode(raw->title, sizeof(raw->title), out->title, "title");
    for (int32_t row = 0; row < MUSIC_COMMENT_ROWS; row++) {
        const uint8_t *const bytes = raw->comment[row];
        M_Decode(bytes, MUSIC_HALF_BYTES, out->comment[row][0], "comment");
        if (strlen((const char *)bytes) > MUSIC_HALF_BYTES) {
            M_Decode(
                bytes + MUSIC_HALF_BYTES, MUSIC_HALF_BYTES,
                out->comment[row][1], "comment");
        }
    }
}

RESULT MusicScreen_ReadComments(
    READER *const reader, MUSIC_TUNE tunes[MUSIC_TUNE_COUNT],
    int32_t *const count)
{
    *count = 0;
    const size_t size = Reader_Remaining(reader);
    uint8_t *const data = Reader_ReadAlloc(reader, size, &g_HeapAllocator);
    MUST(Reader_Result(reader));
    M_RAW_TUNE *const raw = Mem_AllocArrZ(MUSIC_TUNE_COUNT, sizeof(M_RAW_TUNE));
    M_TEXT text = { data, size, 0 };
    int32_t found = 0;
    while (text.position < text.size) {
        if (M_Current(&text) != '@') {
            text.position++;
            continue;
        }
        text.position++;
        if (text.position >= text.size || found >= MUSIC_TUNE_COUNT) {
            break;
        }
        if (!M_ReadTune(&text, &raw[found++])) {
            break;
        }
    }
    for (int32_t i = 0; i < found; i++) {
        M_Convert(&raw[i], &tunes[i]);
    }
    *count = found;
    Mem_Free(raw);
    if (data != nullptr) {
        Allocator_Free(&g_HeapAllocator, data, size);
    }
    return OK;
}
