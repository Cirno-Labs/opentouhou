#include "th01/score_data.h"

#include "core/base/cp932.h"
#include "core/vfs/open.h"

#include <string.h>

static const char *const m_Files[SCORE_RANKS] = {
    "REYHIES.DAT",
    "REYHINO.DAT",
    "REYHIHA.DAT",
    "REYHILU.DAT",
};

static RESULT M_Decode(
    const char *const bytes, const size_t size, char *const out)
{
    char *text;
    MUST(Cp932_Decode(bytes, size, &text));
    char *encoded;
    const RESULT result = Cp932_Encode(text, strlen(text), &encoded);
    if (!IS_OK(result)) {
        Mem_Free(text);
        return result;
    }
    const bool exact =
        strlen(encoded) == size && memcmp(encoded, bytes, size) == 0;
    Mem_Free(encoded);
    if (!exact) {
        Mem_Free(text);
        return FAIL("Noncanonical CP932 in TH01 score file");
    }
    strcpy(out, text);
    Mem_Free(text);
    return OK;
}

static RESULT M_Encode(
    const char *const text, const size_t expected, char *const out)
{
    char *bytes;
    MUST(Cp932_Encode(text, strlen(text), &bytes));
    const size_t size = strlen(bytes);
    if (size != expected) {
        Mem_Free(bytes);
        return FAIL(
            "TH01 score text has %zu bytes, expected %zu", size, expected);
    }
    memcpy(out, bytes, expected);
    Mem_Free(bytes);
    return OK;
}

static void M_Defaults(SCORE_TABLE *const table)
{
    *table = (SCORE_TABLE) {};
    for (int32_t i = 0; i < SCORE_PLACES; i++) {
        strcpy(table->entries[i].name, "東方★靈異伝　　");
        strcpy(table->entries[i].route, "無");
        table->entries[i].score = (SCORE_PLACES - i) * 100;
        table->entries[i].stage = (int16_t)(SCORE_PLACES - i);
    }
}

static RESULT M_Read(READER *const reader, SCORE_TABLE *const out)
{
    SCORE_TABLE table = {};
    char magic[7], names[SCORE_PLACES][16], routes[SCORE_PLACES][2];
    Reader_ReadBytes(reader, magic, sizeof(magic));
    Reader_ReadBytes(reader, names, sizeof(names));
    for (int32_t i = 0; i < SCORE_PLACES; i++) {
        table.entries[i].score = Reader_ReadI32(reader);
    }
    for (int32_t i = 0; i < SCORE_PLACES; i++) {
        table.entries[i].stage = Reader_ReadI16(reader);
    }
    Reader_ReadBytes(reader, routes, sizeof(routes));
    MUST(Reader_Result(reader));
    FAIL_IF(memcmp(magic, "HISC", 4) != 0, "Invalid TH01 score signature");
    for (int32_t i = 0; i < SCORE_PLACES; i++) {
        for (int32_t j = 0; j < 16; j++) {
            names[i][j] = (char)((uint8_t)names[i][j] + 0x64);
        }
        MUST(M_Decode(names[i], 16, table.entries[i].name));
        MUST(M_Decode(routes[i], 2, table.entries[i].route));
    }
    *out = table;
    return OK;
}

static RESULT M_Write(WRITER *const writer, const SCORE_TABLE *const table)
{
    char names[SCORE_PLACES][16], routes[SCORE_PLACES][2];
    for (int32_t i = 0; i < SCORE_PLACES; i++) {
        MUST(M_Encode(table->entries[i].name, 16, names[i]));
        MUST(M_Encode(table->entries[i].route, 2, routes[i]));
        for (int32_t j = 0; j < 16; j++) {
            names[i][j] = (char)((uint8_t)names[i][j] + 0x9c);
        }
    }
    Writer_WriteBytes(writer, "HISCORE", 7);
    Writer_WriteBytes(writer, names, sizeof(names));
    for (int32_t i = 0; i < SCORE_PLACES; i++) {
        Writer_WriteI32(writer, table->entries[i].score);
    }
    for (int32_t i = 0; i < SCORE_PLACES; i++) {
        Writer_WriteI16(writer, table->entries[i].stage);
    }
    Writer_WriteBytes(writer, routes, sizeof(routes));
    return Writer_Result(writer);
}

int32_t ScoreData_Insert(
    SCORE_TABLE *const table, const SCORE_ENTRY *const entry)
{
    int32_t place = 0;
    while (place < SCORE_PLACES && entry->score < table->entries[place].score) {
        place++;
    }
    if (place < SCORE_PLACES) {
        for (int32_t i = SCORE_PLACES - 1; i > place; i--) {
            table->entries[i] = table->entries[i - 1];
        }
        table->entries[place] = *entry;
    }
    return place;
}

RESULT ScoreData_Load(
    VFS *const original, VFS *const user_files, SCORE_SAVES *const out)
{
    *out = (SCORE_SAVES) { .user_files = user_files };
    RESULT first_error = OK;
    for (int32_t rank = 0; rank < SCORE_RANKS; rank++) {
        M_Defaults(&out->tables[rank]);
        const bool native = Vfs_Exists(user_files, m_Files[rank]);
        READER reader;
        RESULT result = OK;
        out->writable[rank] = true;
        out->pending[rank] = !native;
        if (native) {
            result = Vfs_Open(user_files, m_Files[rank], VFS_STREAM, &reader);
        } else if (original != nullptr && Vfs_Exists(original, m_Files[rank])) {
            result = Vfs_Open(original, m_Files[rank], VFS_STREAM, &reader);
        } else {
            continue;
        }
        if (IS_OK(result)) {
            result = M_Read(&reader, &out->tables[rank]);
            Reader_Close(&reader);
        }
        if (!IS_OK(result)) {
            if (native) {
                out->writable[rank] = false;
            }
            if (IS_OK(first_error)) {
                first_error = result;
            } else {
                Result_Ignore(result);
            }
        }
    }
    return first_error;
}

void ScoreData_Queue(SCORE_SAVES *const saves, const int32_t rank)
{
    saves->pending[rank] = true;
}

RESULT ScoreData_WriteSaves(SCORE_SAVES *const saves)
{
    RESULT first_error = OK;
    for (int32_t rank = 0; rank < SCORE_RANKS; rank++) {
        if (!saves->pending[rank]) {
            continue;
        }
        saves->pending[rank] = false;
        if (!saves->writable[rank]) {
            continue;
        }
        WRITER buffer;
        Writer_OpenBuffer(&buffer, &g_HeapAllocator, nullptr);
        RESULT result = M_Write(&buffer, &saves->tables[rank]);
        if (IS_OK(result)) {
            size_t size;
            uint8_t *const bytes = Writer_TakeBuffer(&buffer, &size);
            result = Vfs_Write(saves->user_files, m_Files[rank], bytes, size);
            Allocator_Free(&g_HeapAllocator, bytes, size);
        }
        Result_Ignore(Writer_Close(&buffer));
        if (!IS_OK(result)) {
            if (IS_OK(first_error)) {
                first_error = result;
            } else {
                Result_Ignore(result);
            }
        }
    }
    return first_error;
}
