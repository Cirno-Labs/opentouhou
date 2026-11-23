#include "th06/screens/result/private.h"

#include "th06/game_manager.h"
#include "th06/replay.h"

#include "stg/replay_files.h"

#include "shell/game.h"
#include "core/base/cp932.h"
#include "core/base/log.h"
#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/vfs.h"

#include <stdio.h>
#include <string.h>

#define M_SCORE_PATH "score.dat"
#define M_REPLAY_DIR "replay"
#define M_PENDING_COUNT 2

typedef struct {
    char *path; // in Shell_GetUserFiles
    uint8_t *data;
    size_t size;
} M_PENDING;

typedef struct {
    SCORE_FILE score;
    SAVED_REPLAY slots[SAVED_REPLAY_SLOT_COUNT];
    SAVED_REPLAY user[SAVED_REPLAY_USER_COUNT];
    M_PENDING pending[M_PENDING_COUNT];
    RESULT_SCREEN_CLOCK clock;
    // Counts the scans of the replay directory.
    int32_t scan;
    bool scan_requested;
} M_SAVES;

static M_SAVES m_Saves = {
    .clock = { .date = "01/01/00" },
};
static bool m_RefusesReplays;

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

// Reads the replay of size bytes of data into out, and keeps the bytes for
// playback when the replay is valid.
static void M_ReadReplay(
    const uint8_t *const data, const size_t size, const char *const name,
    const char *const label, SAVED_REPLAY *const out)
{
    *out = (SAVED_REPLAY) {};
    READER reader;
    Reader_OpenBuffer(&reader, data, size, nullptr);
    REPLAY_FILE *file = nullptr;
    if (!Result_Ignore(Replay_Read(&reader, &file))) {
        return;
    }
    char *bytes;
    if (strlen(name) < sizeof(out->path)
        && Result_Ignore(
            Cp932_Encode(file->name, strlen(file->name), &bytes))) {
        out->valid = true;
        memcpy(out->path, name, strlen(name) + 1);
        memcpy(out->label, label, strlen(label) + 1);
        memcpy(out->name, bytes, strlen(bytes) + 1);
        Replay_Summarize(file, &out->summary);
        ReplayFiles_Add(name, Mem_Dup(data, size), size);
        Mem_Free(bytes);
    }
    Replay_Free(file);
}

static void M_SlotLabel(const int32_t slot, char label[8])
{
    snprintf(label, 8, "No.%.2d", slot + 1);
}

static void M_ReadFile(
    const char *const name, const char *const label, SAVED_REPLAY *const out)
{
    *out = (SAVED_REPLAY) {};
    VFS *const files = Shell_GetUserFiles();
    READER reader;
    if (Vfs_Exists(files, name)
        && Result_Log(
            Vfs_Open(files, name, VFS_IN_MEMORY, &reader), LOG_LEVEL_WARNING)) {
        const size_t size = Reader_Remaining(&reader);
        uint8_t *const data = Reader_ReadAlloc(&reader, size, &g_HeapAllocator);
        if (Result_Log(Reader_Result(&reader), LOG_LEVEL_WARNING)) {
            M_ReadReplay(data, size, name, label, out);
        }
        if (data != nullptr) {
            Allocator_Free(&g_HeapAllocator, data, size);
        }
        Reader_Close(&reader);
    }
}

// Returns whether name matches th6_ud????.rpy, ignoring the case of ASCII
// letters as Windows does.
static bool M_IsUserReplay(const char *const name)
{
    const char *const pattern = "th6_ud????.rpy";
    if (strlen(name) != strlen(pattern)) {
        return false;
    }
    for (size_t i = 0; pattern[i] != 0; i++) {
        const char c = name[i] >= 'A' && name[i] <= 'Z'
            ? (char)(name[i] - 'A' + 'a')
            : name[i];
        if (pattern[i] != '?' && c != pattern[i]) {
            return false;
        }
    }
    return true;
}

static void M_ReadUserReplay(void *const user, const char *const name)
{
    int32_t *const found = user;
    if (*found >= SAVED_REPLAY_USER_COUNT || !M_IsUserReplay(name)) {
        return;
    }
    char *const path = Str_Format(M_REPLAY_DIR "/%s", name);
    M_ReadFile(path, "User ", &m_Saves.user[*found]);
    Mem_Free(path);
    (*found)++;
}

static void M_ReadUserReplays(void)
{
    int32_t found = 0;
    Vfs_List(Shell_GetUserFiles(), M_REPLAY_DIR, M_ReadUserReplay, &found);
}

// Forgets the replays read before, then reads the numbered slots and the
// user replays of the replay directory.
static void M_ScanReplays(void)
{
    SAVED_REPLAY *const lists[] = { m_Saves.slots, m_Saves.user };
    const int32_t counts[] = {
        SAVED_REPLAY_SLOT_COUNT,
        SAVED_REPLAY_USER_COUNT,
    };
    for (int32_t list = 0; list < 2; list++) {
        for (int32_t i = 0; i < counts[list]; i++) {
            if (lists[list][i].valid) {
                ReplayFiles_Remove(lists[list][i].path);
            }
            lists[list][i] = (SAVED_REPLAY) {};
        }
    }
    for (int32_t i = 0; i < SAVED_REPLAY_SLOT_COUNT; i++) {
        char *const name = M_ReplayPath(i);
        char label[8];
        M_SlotLabel(i, label);
        M_ReadFile(name, label, &m_Saves.slots[i]);
        Mem_Free(name);
    }
    M_ReadUserReplays();
    m_Saves.scan++;
}

static void M_LoadScore(READER *const reader)
{
    ScoreFile_Free(&m_Saves.score);
    Result_Log(ScoreFile_Read(reader, &m_Saves.score), LOG_LEVEL_INFO);
}

void ResultScreen_OpenSaves(void)
{
    ResultScreen_CloseSaves();
    VFS *const files = Shell_GetUserFiles();
    READER reader;
    if (Vfs_Exists(files, M_SCORE_PATH)
        && Result_Log(
            Vfs_Open(files, M_SCORE_PATH, VFS_IN_MEMORY, &reader),
            LOG_LEVEL_WARNING)) {
        M_LoadScore(&reader);
        Reader_Close(&reader);
    }
    M_ScanReplays();
}

static RESULT M_Write(const M_PENDING *const pending)
{
    return Vfs_Write(
        Shell_GetUserFiles(), pending->path, pending->data, pending->size);
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
    for (int32_t i = 0; i < SAVED_REPLAY_SLOT_COUNT; i++) {
        if (m_Saves.slots[i].valid) {
            ReplayFiles_Remove(m_Saves.slots[i].path);
        }
    }
    for (int32_t i = 0; i < SAVED_REPLAY_USER_COUNT; i++) {
        if (m_Saves.user[i].valid) {
            ReplayFiles_Remove(m_Saves.user[i].path);
        }
    }
    m_Saves.scan_requested = false;
    memset(m_Saves.slots, 0, sizeof(m_Saves.slots));
    memset(m_Saves.user, 0, sizeof(m_Saves.user));
}

void ResultScreen_SetDate(const char *const date)
{
    snprintf(m_Saves.clock.date, sizeof(m_Saves.clock.date), "%s", date);
}

void ResultScreen_CountFrames(const float tick_rate)
{
    if (!g_GameManager.mode.playing) {
        return;
    }
    // The original divides 60 by its game speed, which is 1 at full speed.
    const float target = 60.0f;
    RESULT_SCREEN_CLOCK *const clock = &m_Saves.clock;
    clock->expected_frames += target;
    if (target * 0.9f < tick_rate) {
        clock->achieved_frames += target;
    } else if (target * 0.7f < tick_rate) {
        clock->achieved_frames += target * 0.8f;
    } else if (target * 0.5f < tick_rate) {
        clock->achieved_frames += target * 0.6f;
    } else {
        clock->achieved_frames += target * 0.5f;
    }
}

void ResultScreen_ResetFrames(void)
{
    m_Saves.clock.achieved_frames = 0.0f;
    m_Saves.clock.expected_frames = 0.0f;
}

void ResultScreen_RequestReplayScan(void)
{
    m_Saves.scan_requested = true;
}

void ResultScreen_ScanReplays(void)
{
    if (m_Saves.scan_requested) {
        m_Saves.scan_requested = false;
        M_ScanReplays();
    }
}

int32_t ResultScreen_GetReplayScan(void)
{
    return m_Saves.scan;
}

int32_t ResultScreen_ListReplays(SAVED_REPLAY out[SAVED_REPLAY_COUNT])
{
    int32_t count = 0;
    for (int32_t i = 0; i < SAVED_REPLAY_SLOT_COUNT; i++) {
        if (m_Saves.slots[i].valid) {
            out[count++] = m_Saves.slots[i];
        }
    }
    for (int32_t i = 0; i < SAVED_REPLAY_USER_COUNT; i++) {
        if (m_Saves.user[i].valid) {
            out[count++] = m_Saves.user[i];
        }
    }
    return count;
}

const SCORE_FILE *ResultSaves_GetScoreFile(void)
{
    return &m_Saves.score;
}

void ResultSaves_PutScoreFile(uint8_t *const data, const size_t size)
{
    READER reader;
    Reader_OpenBuffer(&reader, data, size, nullptr);
    M_LoadScore(&reader);
    M_Queue(Str_Dup(M_SCORE_PATH), data, size);
}

const SAVED_REPLAY *ResultSaves_GetSlot(const int32_t slot)
{
    return &m_Saves.slots[slot];
}

void ResultSaves_PutReplay(
    const int32_t slot, uint8_t *const data, const size_t size)
{
    SAVED_REPLAY *const saved = &m_Saves.slots[slot];
    if (saved->valid) {
        ReplayFiles_Remove(saved->path);
    }
    char *const name = M_ReplayPath(slot);
    char label[8];
    M_SlotLabel(slot, label);
    M_ReadReplay(data, size, name, label, saved);
    Mem_Free(name);
    M_Queue(M_ReplayPath(slot), data, size);
}

const RESULT_SCREEN_CLOCK *ResultSaves_GetClock(void)
{
    return &m_Saves.clock;
}

bool ResultSaves_RefusesReplays(void)
{
    return m_RefusesReplays;
}

void ResultScreen_RefuseReplays(const bool refuse)
{
    m_RefusesReplays = refuse;
}
