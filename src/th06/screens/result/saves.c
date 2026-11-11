#include "th06/screens/result/private.h"

#include "th06/replay.h"

#include "core/base/cp932.h"
#include "core/base/log.h"
#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/io/file.h"
#include "core/io/path.h"

#include <string.h>

#define M_SCORE_PATH "score.dat"
#define M_REPLAY_DIR "replay"
#define M_PENDING_COUNT 2

typedef struct {
    char *path; // relative to the user directory
    uint8_t *data;
    size_t size;
} M_PENDING;

typedef struct {
    char *user_dir;
    SCORE_FILE score;
    RESULT_SAVES_SLOT slots[RESULT_SAVES_SLOT_COUNT];
    M_PENDING pending[M_PENDING_COUNT];
    RESULT_SCREEN_CLOCK clock;
} M_SAVES;

static M_SAVES m_Saves = {
    .clock = { "01/01/00", 1.0f, 1.0f },
};

static void M_FreePending(M_PENDING *const pending)
{
    Mem_Free(pending->path);
    if (pending->data != nullptr) {
        Allocator_Free(&g_HeapAllocator, pending->data, pending->size);
    }
    *pending = (M_PENDING) {};
}

static void M_Queue(char *const path, uint8_t *const data, const size_t size)
{
    if (m_Saves.user_dir == nullptr) {
        Mem_Free(path);
        Allocator_Free(&g_HeapAllocator, data, size);
        return;
    }
    M_PENDING *target = nullptr;
    for (int32_t i = 0; i < M_PENDING_COUNT; i++) {
        M_PENDING *const pending = &m_Saves.pending[i];
        if (pending->path != nullptr && strcmp(pending->path, path) == 0) {
            target = pending;
            break;
        }
        if (pending->path == nullptr && target == nullptr) {
            target = pending;
        }
    }
    if (target == nullptr) {
        LOG_WARNING("Dropped the save of %s", path);
        Mem_Free(path);
        Allocator_Free(&g_HeapAllocator, data, size);
        return;
    }
    M_FreePending(target);
    *target = (M_PENDING) { path, data, size };
}

static char *M_ReplayPath(const int32_t slot)
{
    return Str_Format(M_REPLAY_DIR "/th6_%.2d.rpy", slot + 1);
}

static void M_ReadSlot(READER *const reader, RESULT_SAVES_SLOT *const out)
{
    *out = (RESULT_SAVES_SLOT) {};
    REPLAY_FILE *file = nullptr;
    if (!Result_Ignore(Replay_Read(reader, &file))) {
        return;
    }
    char *name;
    if (Result_Ignore(Cp932_Encode(file->name, strlen(file->name), &name))) {
        out->valid = true;
        memcpy(out->name, name, strlen(name) + 1);
        memcpy(out->date, file->date, sizeof(out->date));
        out->shot = file->shot;
        out->score = file->score;
        Mem_Free(name);
    }
    Replay_Free(file);
}

static void M_LoadScore(READER *const reader)
{
    ScoreFile_Free(&m_Saves.score);
    Result_Log(ScoreFile_Read(reader, &m_Saves.score), LOG_LEVEL_INFO);
}

void ResultScreen_OpenSaves(const char *const user_dir)
{
    ResultScreen_CloseSaves();
    if (user_dir == nullptr) {
        return;
    }
    m_Saves.user_dir = Str_Dup(user_dir);
    char *const score_path =
        Path_Join(&g_HeapAllocator, user_dir, M_SCORE_PATH);
    READER reader;
    if (File_Exists(score_path)
        && Result_Log(
            Reader_OpenFile(&reader, score_path), LOG_LEVEL_WARNING)) {
        M_LoadScore(&reader);
        Reader_Close(&reader);
    }
    Mem_Free(score_path);
    for (int32_t i = 0; i < RESULT_SAVES_SLOT_COUNT; i++) {
        char *const name = M_ReplayPath(i);
        char *const path = Path_Join(&g_HeapAllocator, user_dir, name);
        if (File_Exists(path)
            && Result_Log(Reader_OpenFile(&reader, path), LOG_LEVEL_WARNING)) {
            M_ReadSlot(&reader, &m_Saves.slots[i]);
            Reader_Close(&reader);
        }
        Mem_Free(path);
        Mem_Free(name);
    }
}

static RESULT M_Write(const M_PENDING *const pending)
{
    char *const path =
        Path_Join(&g_HeapAllocator, m_Saves.user_dir, pending->path);
    RESULT result = OK;
    if (strncmp(pending->path, M_REPLAY_DIR "/", strlen(M_REPLAY_DIR) + 1)
        == 0) {
        char *const directory =
            Path_Join(&g_HeapAllocator, m_Saves.user_dir, M_REPLAY_DIR);
        result = File_MakeDir(directory);
        Mem_Free(directory);
    }
    WRITER writer;
    if (IS_OK(result)) {
        result = Writer_OpenFile(&writer, path);
    }
    if (IS_OK(result)) {
        Writer_WriteBytes(&writer, pending->data, pending->size);
        result = Writer_Close(&writer);
    }
    Mem_Free(path);
    return result;
}

RESULT ResultScreen_WriteSaves(void)
{
    RESULT first = OK;
    for (int32_t i = 0; i < M_PENDING_COUNT; i++) {
        M_PENDING *const pending = &m_Saves.pending[i];
        if (pending->path == nullptr) {
            continue;
        }
        const RESULT result = M_Write(pending);
        if (IS_OK(first)) {
            first = result;
        } else {
            Result_Log(result, LOG_LEVEL_WARNING);
        }
        M_FreePending(pending);
    }
    return first;
}

void ResultScreen_CloseSaves(void)
{
    for (int32_t i = 0; i < M_PENDING_COUNT; i++) {
        M_FreePending(&m_Saves.pending[i]);
    }
    ScoreFile_Free(&m_Saves.score);
    Mem_Free(m_Saves.user_dir);
    m_Saves.user_dir = nullptr;
    memset(m_Saves.slots, 0, sizeof(m_Saves.slots));
}

void ResultScreen_SetClock(const RESULT_SCREEN_CLOCK *const clock)
{
    m_Saves.clock = *clock;
}

const SCORE_FILE *ResultSaves_GetScoreFile(void)
{
    return &m_Saves.score;
}

void ResultSaves_PutScoreFile(uint8_t *const data, const size_t size)
{
    READER reader;
    Reader_OpenBuffer(&reader, data, size);
    M_LoadScore(&reader);
    M_Queue(Str_Dup(M_SCORE_PATH), data, size);
}

const RESULT_SAVES_SLOT *ResultSaves_GetSlot(const int32_t slot)
{
    return &m_Saves.slots[slot];
}

void ResultSaves_PutReplay(
    const int32_t slot, uint8_t *const data, const size_t size)
{
    READER reader;
    Reader_OpenBuffer(&reader, data, size);
    M_ReadSlot(&reader, &m_Saves.slots[slot]);
    M_Queue(M_ReplayPath(slot), data, size);
}

const RESULT_SCREEN_CLOCK *ResultSaves_GetClock(void)
{
    return &m_Saves.clock;
}
