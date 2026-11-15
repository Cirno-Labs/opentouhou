#include "th07/screens/result/saves.h"

#include "th07/replay.h"

#include "core/base/log.h"
#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/io/file.h"
#include "core/io/path.h"

#include <stdio.h>
#include <string.h>

#define M_REPLAY_DIR "replay"

typedef struct {
    char *user_dir;
    SAVED_REPLAY slots[SAVED_REPLAY_SLOT_COUNT];
    SAVED_REPLAY user[SAVED_REPLAY_USER_COUNT];
    // Counts the scans of the replay directory.
    int32_t scan;
    bool scan_requested;
} M_SAVES;

static M_SAVES m_Saves;

// Returns the path of name, relative to the user directory, as the
// replay module opens it.
static char *M_FullPath(const char *const name)
{
    return m_Saves.user_dir != nullptr
        ? Path_Join(&g_HeapAllocator, m_Saves.user_dir, name)
        : Str_Dup(name);
}

// Reads the replay of size bytes of data into out, and keeps the bytes for
// playback when the replay is valid.
static void M_ReadReplay(
    const uint8_t *const data, const size_t size, const char *const path,
    const char *const label, SAVED_REPLAY *const out)
{
    READER reader;
    Reader_OpenBuffer(&reader, data, size, nullptr);
    REPLAY_FILE *file = nullptr;
    if (!Result_Ignore(Replay_Read(&reader, &file))) {
        return;
    }
    if (strlen(path) < sizeof(out->path)) {
        out->valid = true;
        memcpy(out->path, path, strlen(path) + 1);
        snprintf(out->label, sizeof(out->label), "%s", label);
        Replay_Summarize(file, &out->summary);
        Replay_AddFile(path, data, size);
    }
    Replay_Free(file);
}

static void M_ReadFile(
    const char *const name, const char *const label, SAVED_REPLAY *const out)
{
    *out = (SAVED_REPLAY) {};
    char *const path = M_FullPath(name);
    READER reader;
    if (File_Exists(path)
        && Result_Log(Reader_OpenFile(&reader, path), LOG_LEVEL_WARNING)) {
        const size_t size = Reader_Remaining(&reader);
        uint8_t *const data = Reader_ReadAlloc(&reader, size, &g_HeapAllocator);
        if (Result_Log(Reader_Result(&reader), LOG_LEVEL_WARNING)) {
            M_ReadReplay(data, size, path, label, out);
        }
        if (data != nullptr) {
            Allocator_Free(&g_HeapAllocator, data, size);
        }
        Reader_Close(&reader);
    }
    Mem_Free(path);
}

// Returns whether name matches th7_ud????.rpy, ignoring the case of ASCII
// letters as Windows does.
static bool M_IsUserReplay(const char *const name)
{
    const char *const pattern = "th7_ud????.rpy";
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

static void M_ReadUserReplays(void)
{
    char *const directory = M_FullPath(M_REPLAY_DIR);
    FILE_LIST list;
    if (File_Exists(directory)
        && Result_Log(
            File_ListDir(directory, &g_HeapAllocator, &list),
            LOG_LEVEL_WARNING)) {
        int32_t found = 0;
        for (int32_t i = 0; i < list.count && found < SAVED_REPLAY_USER_COUNT;
             i++) {
            if (!M_IsUserReplay(list.items[i])) {
                continue;
            }
            char *const name = Str_Format(M_REPLAY_DIR "/%s", list.items[i]);
            M_ReadFile(name, "User ", &m_Saves.user[found]);
            Mem_Free(name);
            found++;
        }
        File_FreeList(&list);
    }
    Mem_Free(directory);
}

static void M_Forget(SAVED_REPLAY *const replays, const int32_t count)
{
    for (int32_t i = 0; i < count; i++) {
        if (replays[i].valid) {
            Replay_RemoveFile(replays[i].path);
        }
        replays[i] = (SAVED_REPLAY) {};
    }
}

static void M_ScanReplays(void)
{
    M_Forget(m_Saves.slots, SAVED_REPLAY_SLOT_COUNT);
    M_Forget(m_Saves.user, SAVED_REPLAY_USER_COUNT);
    for (int32_t i = 0; i < SAVED_REPLAY_SLOT_COUNT; i++) {
        char *const name = Str_Format(M_REPLAY_DIR "/th7_%.2d.rpy", i + 1);
        char label[8];
        snprintf(label, sizeof(label), "No.%.2d", i + 1);
        M_ReadFile(name, label, &m_Saves.slots[i]);
        Mem_Free(name);
    }
    M_ReadUserReplays();
    m_Saves.scan++;
}

void ResultScreen_OpenSaves(const char *const user_dir)
{
    ResultScreen_CloseSaves();
    if (user_dir == nullptr) {
        return;
    }
    m_Saves.user_dir = Str_Dup(user_dir);
    M_ScanReplays();
}

void ResultScreen_CloseSaves(void)
{
    M_Forget(m_Saves.slots, SAVED_REPLAY_SLOT_COUNT);
    M_Forget(m_Saves.user, SAVED_REPLAY_USER_COUNT);
    Mem_Free(m_Saves.user_dir);
    m_Saves.user_dir = nullptr;
    m_Saves.scan_requested = false;
}

void ResultScreen_RequestReplayScan(void)
{
    m_Saves.scan_requested = true;
}

void ResultScreen_ScanReplays(void)
{
    if (m_Saves.scan_requested) {
        m_Saves.scan_requested = false;
        if (m_Saves.user_dir != nullptr) {
            M_ScanReplays();
        }
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
