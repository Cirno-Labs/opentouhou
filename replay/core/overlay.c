#include "overlay.h"

#include "iat.h"

#include "core/base/log.h"
#include "core/base/memory.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

// Maps a path under the game directory G to the same path under the output
// directory O. Writes go to O; reads take O's file when it has one, else
// G's. A write that needs the content of a file only G has fails, so no
// file of G is ever copied or changed.

#define MAX_FIND_HANDLES 64

// Matches the access rights with which an open can change a file.
#define WRITE_ACCESS                                                           \
    (GENERIC_WRITE | GENERIC_ALL | FILE_WRITE_DATA | FILE_APPEND_DATA)

typedef HANDLE(WINAPI *CREATEFILEA_FN)(
    LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
typedef BOOL(WINAPI *CREATEDIRECTORYA_FN)(LPCSTR, LPSECURITY_ATTRIBUTES);
typedef BOOL(WINAPI *DELETEFILEA_FN)(LPCSTR);
typedef HANDLE(WINAPI *FINDFIRSTFILEA_FN)(LPCSTR, LPWIN32_FIND_DATAA);
typedef BOOL(WINAPI *FINDNEXTFILEA_FN)(HANDLE, LPWIN32_FIND_DATAA);
typedef BOOL(WINAPI *FINDCLOSE_FN)(HANDLE);

typedef struct {
    WIN32_FIND_DATAA *entries;
    DWORD count;
    DWORD index;
} FIND_STATE;

static wchar_t m_GameDirW[MAX_PATH];
static wchar_t m_OutDirW[MAX_PATH];
static char m_GameDirA[MAX_PATH];
static size_t m_GameDirALen;

static CRITICAL_SECTION m_Lock;
static FIND_STATE *m_FindHandles[MAX_FIND_HANDLES];

static CREATEFILEA_FN m_RealCreateFileA;
static CREATEDIRECTORYA_FN m_RealCreateDirectoryA;
static DELETEFILEA_FN m_RealDeleteFileA;
static FINDFIRSTFILEA_FN m_RealFindFirstFileA;
static FINDNEXTFILEA_FN m_RealFindNextFileA;
static FINDCLOSE_FN m_RealFindClose;

static void M_LogCall(const char *func, const char *path, const char *verdict)
{
    LOG_INFO("%s %s %s", func, path, verdict);
}

static bool M_IsUnderGame(const char *full)
{
    if (_strnicmp(full, m_GameDirA, m_GameDirALen) != 0) {
        return false;
    }
    char next = full[m_GameDirALen];
    return next == '\0' || next == '\\';
}

static void M_MapToOut(const char *full, wchar_t *out_buf, size_t out_cap)
{
    const char *relative = full + m_GameDirALen;
    while (*relative == '\\') {
        relative++;
    }
    if (*relative == '\0') {
        wcsncpy(out_buf, m_OutDirW, out_cap);
        out_buf[out_cap - 1] = L'\0';
        return;
    }
    wchar_t relative_w[MAX_PATH];
    MultiByteToWideChar(
        CP_ACP, 0, relative, -1, relative_w,
        sizeof(relative_w) / sizeof(relative_w[0]));
    swprintf(out_buf, (int)out_cap, L"%ls\\%ls", m_OutDirW, relative_w);
}

static void M_CreateDirRecursiveW(const wchar_t *dir_path)
{
    wchar_t buf[MAX_PATH];
    size_t length = wcslen(dir_path);
    if (length >= MAX_PATH) {
        return;
    }
    wcscpy(buf, dir_path);
    size_t start = wcslen(m_OutDirW) + 1;
    if (length < start) {
        return;
    }
    for (size_t i = start; i <= length; i++) {
        if (i == length || buf[i] == L'\\') {
            wchar_t saved = buf[i];
            buf[i] = L'\0';
            CreateDirectoryW(buf, nullptr);
            buf[i] = saved;
        }
    }
}

static void M_EnsureParentDirsW(const wchar_t *file_path)
{
    wchar_t parent[MAX_PATH];
    wcsncpy(parent, file_path, MAX_PATH);
    parent[MAX_PATH - 1] = L'\0';
    wchar_t *last_sep = wcsrchr(parent, L'\\');
    if (last_sep == nullptr) {
        return;
    }
    *last_sep = L'\0';
    M_CreateDirRecursiveW(parent);
}

static bool M_FileExistsW(const wchar_t *path)
{
    return GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES;
}

static bool M_FileExistsA(const char *path)
{
    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
}

static HANDLE WINAPI M_CreateFileA(
    LPCSTR path, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES sa,
    DWORD disposition, DWORD flags, HANDLE template_file)
{
    char full[MAX_PATH];
    if (GetFullPathNameA(path, MAX_PATH, full, nullptr) == 0
        || !M_IsUnderGame(full)) {
        return m_RealCreateFileA(
            path, access, share, sa, disposition, flags, template_file);
    }

    wchar_t out_path[MAX_PATH];
    M_MapToOut(full, out_path, MAX_PATH);

    bool wants_write_or_create = (access & WRITE_ACCESS) != 0
        || disposition == CREATE_ALWAYS || disposition == CREATE_NEW
        || disposition == OPEN_ALWAYS || disposition == TRUNCATE_EXISTING;
    bool needs_existing_content =
        (disposition == OPEN_EXISTING || disposition == OPEN_ALWAYS
         || disposition == TRUNCATE_EXISTING)
        && (access & WRITE_ACCESS) != 0;

    if (!wants_write_or_create) {
        if (M_FileExistsW(out_path)) {
            HANDLE handle = CreateFileW(
                out_path, access, share, sa, disposition, flags, template_file);
            M_LogCall("CreateFileA", path, "out");
            return handle;
        }
        M_LogCall("CreateFileA", path, "game");
        return m_RealCreateFileA(
            path, access, share, sa, disposition, flags, template_file);
    }

    if (needs_existing_content && !M_FileExistsW(out_path)
        && M_FileExistsA(full)) {
        M_LogCall("CreateFileA", path, "refused");
        SetLastError(ERROR_ACCESS_DENIED);
        return INVALID_HANDLE_VALUE;
    }

    M_EnsureParentDirsW(out_path);
    HANDLE handle = CreateFileW(
        out_path, access, share, sa, disposition, flags, template_file);
    M_LogCall("CreateFileA", path, "out");
    return handle;
}

static BOOL WINAPI M_CreateDirectoryA(LPCSTR path, LPSECURITY_ATTRIBUTES sa)
{
    char full[MAX_PATH];
    if (GetFullPathNameA(path, MAX_PATH, full, nullptr) == 0
        || !M_IsUnderGame(full)) {
        return m_RealCreateDirectoryA(path, sa);
    }

    wchar_t out_path[MAX_PATH];
    M_MapToOut(full, out_path, MAX_PATH);
    M_EnsureParentDirsW(out_path);
    BOOL result = CreateDirectoryW(out_path, sa);
    M_LogCall("CreateDirectoryA", path, "out");
    return result;
}

static BOOL WINAPI M_DeleteFileA(LPCSTR path)
{
    char full[MAX_PATH];
    if (GetFullPathNameA(path, MAX_PATH, full, nullptr) == 0
        || !M_IsUnderGame(full)) {
        return m_RealDeleteFileA(path);
    }

    wchar_t out_path[MAX_PATH];
    M_MapToOut(full, out_path, MAX_PATH);
    if (!M_FileExistsW(out_path)) {
        M_LogCall("DeleteFileA", path, "refused");
        SetLastError(ERROR_FILE_NOT_FOUND);
        return FALSE;
    }
    BOOL result = DeleteFileW(out_path);
    M_LogCall("DeleteFileA", path, "out");
    return result;
}

static void M_CopyFindDataW(const WIN32_FIND_DATAW *src, WIN32_FIND_DATAA *dst)
{
    dst->dwFileAttributes = src->dwFileAttributes;
    dst->ftCreationTime = src->ftCreationTime;
    dst->ftLastAccessTime = src->ftLastAccessTime;
    dst->ftLastWriteTime = src->ftLastWriteTime;
    dst->nFileSizeHigh = src->nFileSizeHigh;
    dst->nFileSizeLow = src->nFileSizeLow;
    dst->dwReserved0 = src->dwReserved0;
    dst->dwReserved1 = src->dwReserved1;
    WideCharToMultiByte(
        CP_ACP, 0, src->cFileName, -1, dst->cFileName, sizeof(dst->cFileName),
        nullptr, nullptr);
    dst->cAlternateFileName[0] = '\0';
}

static bool M_HasName(
    const WIN32_FIND_DATAA *entries, DWORD count, const char *name)
{
    for (DWORD i = 0; i < count; i++) {
        if (_stricmp(entries[i].cFileName, name) == 0) {
            return true;
        }
    }
    return false;
}

static HANDLE WINAPI M_FindFirstFileA(LPCSTR pattern, LPWIN32_FIND_DATAA data)
{
    char full[MAX_PATH];
    if (GetFullPathNameA(pattern, MAX_PATH, full, nullptr) == 0
        || !M_IsUnderGame(full)) {
        return m_RealFindFirstFileA(pattern, data);
    }

    wchar_t out_pattern[MAX_PATH];
    M_MapToOut(full, out_pattern, MAX_PATH);

    WIN32_FIND_DATAA entries[256];
    DWORD count = 0;

    WIN32_FIND_DATAW out_data;
    HANDLE out_handle = FindFirstFileW(out_pattern, &out_data);
    if (out_handle != INVALID_HANDLE_VALUE) {
        do {
            if (count < ARRAYSIZE(entries)) {
                M_CopyFindDataW(&out_data, &entries[count]);
                count++;
            }
        } while (FindNextFileW(out_handle, &out_data));
        FindClose(out_handle);
    }

    WIN32_FIND_DATAA game_data;
    HANDLE game_handle = m_RealFindFirstFileA(full, &game_data);
    if (game_handle != INVALID_HANDLE_VALUE) {
        do {
            if (count < ARRAYSIZE(entries)
                && !M_HasName(entries, count, game_data.cFileName)) {
                entries[count] = game_data;
                count++;
            }
        } while (m_RealFindNextFileA(game_handle, &game_data));
        m_RealFindClose(game_handle);
    }

    M_LogCall("FindFirstFileA", pattern, count > 0 ? "out" : "none");

    if (count == 0) {
        SetLastError(ERROR_FILE_NOT_FOUND);
        return INVALID_HANDLE_VALUE;
    }

    FIND_STATE *state = Mem_Alloc(sizeof(FIND_STATE));
    state->entries = Mem_Alloc(sizeof(WIN32_FIND_DATAA) * count);
    memcpy(state->entries, entries, sizeof(WIN32_FIND_DATAA) * count);
    state->count = count;
    state->index = 1;

    bool registered = false;
    EnterCriticalSection(&m_Lock);
    for (int i = 0; i < MAX_FIND_HANDLES && !registered; i++) {
        if (m_FindHandles[i] == nullptr) {
            m_FindHandles[i] = state;
            registered = true;
        }
    }
    LeaveCriticalSection(&m_Lock);
    if (!registered) {
        Mem_Free(state->entries);
        Mem_Free(state);
        M_LogCall("FindFirstFileA", pattern, "too many open searches");
        SetLastError(ERROR_NO_MORE_SEARCH_HANDLES);
        return INVALID_HANDLE_VALUE;
    }
    *data = state->entries[0];
    return (HANDLE)state;
}

static bool M_IsOurFindHandle(HANDLE handle)
{
    bool found = false;
    EnterCriticalSection(&m_Lock);
    for (int i = 0; i < MAX_FIND_HANDLES; i++) {
        if ((HANDLE)m_FindHandles[i] == handle) {
            found = true;
            break;
        }
    }
    LeaveCriticalSection(&m_Lock);
    return found;
}

static BOOL WINAPI M_FindNextFileA(HANDLE handle, LPWIN32_FIND_DATAA data)
{
    if (!M_IsOurFindHandle(handle)) {
        return m_RealFindNextFileA(handle, data);
    }
    FIND_STATE *state = (FIND_STATE *)handle;
    if (state->index >= state->count) {
        SetLastError(ERROR_NO_MORE_FILES);
        return FALSE;
    }
    *data = state->entries[state->index];
    state->index++;
    return TRUE;
}

static BOOL WINAPI M_FindClose(HANDLE handle)
{
    if (!M_IsOurFindHandle(handle)) {
        return m_RealFindClose(handle);
    }
    FIND_STATE *state = (FIND_STATE *)handle;
    EnterCriticalSection(&m_Lock);
    for (int i = 0; i < MAX_FIND_HANDLES; i++) {
        if (m_FindHandles[i] == state) {
            m_FindHandles[i] = nullptr;
            break;
        }
    }
    LeaveCriticalSection(&m_Lock);
    Mem_Free(state->entries);
    Mem_Free(state);
    return TRUE;
}

RESULT Overlay_Install(const wchar_t *game_dir, const wchar_t *out_dir)
{
    InitializeCriticalSection(&m_Lock);
    wcsncpy(m_GameDirW, game_dir, MAX_PATH);
    m_GameDirW[MAX_PATH - 1] = L'\0';
    wcsncpy(m_OutDirW, out_dir, MAX_PATH);
    m_OutDirW[MAX_PATH - 1] = L'\0';
    WideCharToMultiByte(
        CP_ACP, 0, m_GameDirW, -1, m_GameDirA, sizeof(m_GameDirA), nullptr,
        nullptr);
    m_GameDirALen = strlen(m_GameDirA);

    HMODULE module = GetModuleHandleW(nullptr);

    struct {
        const char *name;
        uintptr_t hook;
        uintptr_t *real_slot;
    } table[] = {
        { "CreateFileA", (uintptr_t)M_CreateFileA,
          (uintptr_t *)&m_RealCreateFileA },
        { "CreateDirectoryA", (uintptr_t)M_CreateDirectoryA,
          (uintptr_t *)&m_RealCreateDirectoryA },
        { "DeleteFileA", (uintptr_t)M_DeleteFileA,
          (uintptr_t *)&m_RealDeleteFileA },
        { "FindFirstFileA", (uintptr_t)M_FindFirstFileA,
          (uintptr_t *)&m_RealFindFirstFileA },
        { "FindNextFileA", (uintptr_t)M_FindNextFileA,
          (uintptr_t *)&m_RealFindNextFileA },
        { "FindClose", (uintptr_t)M_FindClose, (uintptr_t *)&m_RealFindClose },
    };

    for (size_t i = 0; i < ARRAYSIZE(table); i++) {
        if (!Iat_Patch(
                module, "KERNEL32.dll", table[i].name, table[i].hook,
                table[i].real_slot)) {
            return FAIL("overlay: %s is not imported", table[i].name);
        }
    }
    return OK;
}
