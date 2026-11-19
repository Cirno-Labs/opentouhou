#include "overlay.h"

#include "iat.h"

#include "core/base/log.h"
#include "core/base/memory.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

// Maps a path under the game directory G to the same path under the output
// directory O. Writes go to O; reads take O's file when it has one, else
// G's. A write that needs the content of a file only G has fails, so no
// file of G is ever copied or changed. A write outside G fails.
//
// A staged file, which the driver gives from memory, lies between O and
// G: a read takes it unless O has the file, through a handle that the
// hooked file functions serve from memory.
//
// A change of directory into a directory that only O has keeps the real
// working directory and resolves relative paths against the directory in
// G's terms. Without this, the original's "cd replay" fails and its next
// "cd .." leaves G.
//
// Only O gives the private paths, files or directories below G, so a
// scripted run starts without the player's saves.

#define MAX_FIND_HANDLES 64
#define MAX_MEMORY_HANDLES 16

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
typedef BOOL(WINAPI *SETCURRENTDIRECTORYA_FN)(LPCSTR);
typedef DWORD(WINAPI *GETCURRENTDIRECTORYA_FN)(DWORD, LPSTR);

typedef BOOL(WINAPI *READFILE_FN)(HANDLE, LPVOID, DWORD, LPDWORD, LPOVERLAPPED);
typedef DWORD(WINAPI *GETFILESIZE_FN)(HANDLE, LPDWORD);
typedef DWORD(WINAPI *SETFILEPOINTER_FN)(HANDLE, LONG, PLONG, DWORD);
typedef DWORD(WINAPI *GETFILETYPE_FN)(HANDLE);
typedef BOOL(WINAPI *GETFILETIME_FN)(
    HANDLE, LPFILETIME, LPFILETIME, LPFILETIME);
typedef BOOL(WINAPI *CLOSEHANDLE_FN)(HANDLE);

typedef struct {
    WIN32_FIND_DATAA *entries;
    DWORD count;
    DWORD index;
} FIND_STATE;

// Holds an open staged file; handle is an event that only names it.
typedef struct {
    HANDLE handle;
    const STAGED_FILE *file;
    DWORD position;
} MEMORY_HANDLE;

static wchar_t m_GameDirW[MAX_PATH];
static wchar_t m_WorkDirW[MAX_PATH];
static char m_GameDirA[MAX_PATH];
static size_t m_GameDirALen;
static char m_VirtualDir[MAX_PATH];
static const char *const *m_PrivatePaths;
static size_t m_PrivateCount;

static CRITICAL_SECTION m_Lock;
static FIND_STATE *m_FindHandles[MAX_FIND_HANDLES];
static const STAGED_FILE *m_Staged;
static size_t m_StagedCount;
static MEMORY_HANDLE m_MemoryHandles[MAX_MEMORY_HANDLES];

static CREATEFILEA_FN m_RealCreateFileA;
static CREATEDIRECTORYA_FN m_RealCreateDirectoryA;
static DELETEFILEA_FN m_RealDeleteFileA;
static FINDFIRSTFILEA_FN m_RealFindFirstFileA;
static FINDNEXTFILEA_FN m_RealFindNextFileA;
static FINDCLOSE_FN m_RealFindClose;
static SETCURRENTDIRECTORYA_FN m_RealSetCurrentDirectoryA;
static GETCURRENTDIRECTORYA_FN m_RealGetCurrentDirectoryA;
static READFILE_FN m_RealReadFile;
static GETFILESIZE_FN m_RealGetFileSize;
static SETFILEPOINTER_FN m_RealSetFilePointer;
static GETFILETYPE_FN m_RealGetFileType;
static GETFILETIME_FN m_RealGetFileTime;
static CLOSEHANDLE_FN m_RealCloseHandle;

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

static bool M_IsRelative(const char *path)
{
    return path[0] != '\\' && path[0] != '/' && path[0] != '\0'
        && path[1] != ':';
}

// Returns the part of full, a path under the game directory, below it.
static const char *M_Relative(const char *const full)
{
    const char *relative = full + m_GameDirALen;
    while (*relative == '\\') {
        relative++;
    }
    return relative;
}

static bool M_IsPrivate(const char *const full)
{
    const char *const relative = M_Relative(full);
    for (size_t i = 0; i < m_PrivateCount; i++) {
        const size_t length = strlen(m_PrivatePaths[i]);
        if (_strnicmp(relative, m_PrivatePaths[i], length) == 0
            && (relative[length] == '\0' || relative[length] == '\\')) {
            return true;
        }
    }
    return false;
}

// Returns whether the first length bytes of a and b match, '/' matching
// '\\' and ASCII letters of either case.
static bool M_SamePath(const char *a, const char *b, size_t length)
{
    for (; length > 0; length--, a++, b++) {
        const char ca = *a == '\\' ? '/' : (char)tolower((unsigned char)*a);
        const char cb = *b == '\\' ? '/' : (char)tolower((unsigned char)*b);
        if (ca != cb) {
            return false;
        }
        if (ca == '\0') {
            return true;
        }
    }
    return true;
}

// Converts text from the code page of codepage to that of to, into out.
static void M_Recode(
    const char *const text, const UINT codepage, const UINT to,
    char out[MAX_PATH * 3])
{
    wchar_t wide[MAX_PATH];
    out[0] = '\0';
    if (MultiByteToWideChar(codepage, 0, text, -1, wide, MAX_PATH) != 0) {
        WideCharToMultiByte(
            to, 0, wide, -1, out, MAX_PATH * 3, nullptr, nullptr);
    }
}

// Returns the staged file at full, a path under the game directory in the
// ANSI code page; staged paths are UTF-8.
static const STAGED_FILE *M_FindStaged(const char *const full)
{
    char relative[MAX_PATH * 3];
    M_Recode(M_Relative(full), CP_ACP, CP_UTF8, relative);
    for (size_t i = m_StagedCount; i > 0; i--) {
        if (M_SamePath(m_Staged[i - 1].path, relative, SIZE_MAX)) {
            return &m_Staged[i - 1];
        }
    }
    return nullptr;
}

static HANDLE M_OpenStaged(const STAGED_FILE *const file)
{
    HANDLE handle = INVALID_HANDLE_VALUE;
    EnterCriticalSection(&m_Lock);
    for (int i = 0; i < MAX_MEMORY_HANDLES; i++) {
        if (m_MemoryHandles[i].handle == nullptr) {
            handle = CreateEventW(nullptr, TRUE, FALSE, nullptr);
            if (handle != nullptr) {
                m_MemoryHandles[i] =
                    (MEMORY_HANDLE) { .handle = handle, .file = file };
            } else {
                handle = INVALID_HANDLE_VALUE;
            }
            break;
        }
    }
    LeaveCriticalSection(&m_Lock);
    if (handle == INVALID_HANDLE_VALUE) {
        SetLastError(ERROR_TOO_MANY_OPEN_FILES);
    }
    return handle;
}

// Returns the open staged file of handle, or nullptr for any other handle.
static MEMORY_HANDLE *M_FindMemory(const HANDLE handle)
{
    MEMORY_HANDLE *found = nullptr;
    EnterCriticalSection(&m_Lock);
    for (int i = 0; i < MAX_MEMORY_HANDLES && found == nullptr; i++) {
        if (handle != nullptr && m_MemoryHandles[i].handle == handle) {
            found = &m_MemoryHandles[i];
        }
    }
    LeaveCriticalSection(&m_Lock);
    return found;
}

static bool M_FullPath(const char *path, char full[MAX_PATH])
{
    char joined[MAX_PATH];
    if (m_VirtualDir[0] != '\0' && M_IsRelative(path)) {
        const int length =
            snprintf(joined, sizeof(joined), "%s\\%s", m_VirtualDir, path);
        if (length < 0 || length >= MAX_PATH) {
            return false;
        }
        path = joined;
    }
    const DWORD length = GetFullPathNameA(path, MAX_PATH, full, nullptr);
    return length != 0 && length < MAX_PATH;
}

static void M_MapToOut(const char *full, wchar_t *out_buf, size_t out_cap)
{
    const char *relative = full + m_GameDirALen;
    while (*relative == '\\') {
        relative++;
    }
    if (*relative == '\0') {
        wcsncpy(out_buf, m_WorkDirW, out_cap);
        out_buf[out_cap - 1] = L'\0';
        return;
    }
    wchar_t relative_w[MAX_PATH];
    MultiByteToWideChar(
        CP_ACP, 0, relative, -1, relative_w,
        sizeof(relative_w) / sizeof(relative_w[0]));
    swprintf(out_buf, (int)out_cap, L"%ls\\%ls", m_WorkDirW, relative_w);
}

static void M_CreateDirRecursiveW(const wchar_t *dir_path)
{
    wchar_t buf[MAX_PATH];
    size_t length = wcslen(dir_path);
    if (length >= MAX_PATH) {
        return;
    }
    wcscpy(buf, dir_path);
    size_t start = wcslen(m_WorkDirW) + 1;
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
    const bool wants_write_or_create = (access & WRITE_ACCESS) != 0
        || disposition == CREATE_ALWAYS || disposition == CREATE_NEW
        || disposition == OPEN_ALWAYS || disposition == TRUNCATE_EXISTING;
    char full[MAX_PATH];
    if (!M_FullPath(path, full)) {
        if (wants_write_or_create) {
            M_LogCall("CreateFileA", path, "refused");
            SetLastError(ERROR_ACCESS_DENIED);
            return INVALID_HANDLE_VALUE;
        }
        return m_RealCreateFileA(
            path, access, share, sa, disposition, flags, template_file);
    }
    if (!M_IsUnderGame(full)) {
        if (wants_write_or_create) {
            M_LogCall("CreateFileA", full, "refused");
            SetLastError(ERROR_ACCESS_DENIED);
            return INVALID_HANDLE_VALUE;
        }
        return m_RealCreateFileA(
            full, access, share, sa, disposition, flags, template_file);
    }

    wchar_t out_path[MAX_PATH];
    M_MapToOut(full, out_path, MAX_PATH);

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
        const STAGED_FILE *const staged = M_FindStaged(full);
        if (staged != nullptr) {
            M_LogCall("CreateFileA", path, "staged");
            return M_OpenStaged(staged);
        }
        if (M_IsPrivate(full)) {
            SetLastError(ERROR_FILE_NOT_FOUND);
            return INVALID_HANDLE_VALUE;
        }
        M_LogCall("CreateFileA", path, "game");
        return m_RealCreateFileA(
            full, access, share, sa, disposition, flags, template_file);
    }

    if (needs_existing_content && !M_FileExistsW(out_path)
        && (M_FileExistsA(full) || M_FindStaged(full) != nullptr)) {
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
    if (!M_FullPath(path, full) || !M_IsUnderGame(full)) {
        M_LogCall("CreateDirectoryA", path, "refused");
        SetLastError(ERROR_ACCESS_DENIED);
        return FALSE;
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
    if (!M_FullPath(path, full) || !M_IsUnderGame(full)) {
        M_LogCall("DeleteFileA", path, "refused");
        SetLastError(ERROR_ACCESS_DENIED);
        return FALSE;
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

// Returns whether name matches pattern, with '*' and '?' as Windows reads
// them, without regard to case.
static bool M_Matches(const char *const pattern, const char *const name)
{
    if (*pattern == '\0') {
        return *name == '\0';
    }
    if (*pattern == '*') {
        for (const char *rest = name;; rest++) {
            if (M_Matches(pattern + 1, rest)) {
                return true;
            }
            if (*rest == '\0') {
                return false;
            }
        }
    }
    return *name != '\0'
        && (*pattern == '?'
            || CharLowerA((LPSTR)(uintptr_t)(unsigned char)*pattern)
                == CharLowerA((LPSTR)(uintptr_t)(unsigned char)*name))
        && M_Matches(pattern + 1, name + 1);
}

// Adds each staged file that full, a search pattern under the game
// directory, matches and entries lacks.
static void M_AddStagedEntries(
    const char *const full, WIN32_FIND_DATAA *const entries, DWORD *const count,
    const DWORD capacity)
{
    char relative[MAX_PATH * 3];
    M_Recode(M_Relative(full), CP_ACP, CP_UTF8, relative);
    const char *const slash = strrchr(relative, '\\');
    const size_t dir_length = slash != nullptr ? (size_t)(slash - relative) : 0;
    const char *const pattern = slash != nullptr ? slash + 1 : relative;
    for (size_t i = 0; i < m_StagedCount && *count < capacity; i++) {
        const STAGED_FILE *const file = &m_Staged[i];
        const char *const file_slash = strrchr(file->path, '/');
        const size_t file_dir_length =
            file_slash != nullptr ? (size_t)(file_slash - file->path) : 0;
        const char *const name =
            file_slash != nullptr ? file_slash + 1 : file->path;
        char ansi[MAX_PATH * 3];
        M_Recode(name, CP_UTF8, CP_ACP, ansi);
        if (file_dir_length != dir_length
            || !M_SamePath(file->path, relative, dir_length)
            || !M_Matches(pattern, name) || M_HasName(entries, *count, ansi)) {
            continue;
        }
        WIN32_FIND_DATAA *const entry = &entries[(*count)++];
        memset(entry, 0, sizeof(*entry));
        entry->dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
        entry->nFileSizeLow = (DWORD)file->size;
        snprintf(entry->cFileName, sizeof(entry->cFileName), "%s", ansi);
    }
}

static HANDLE WINAPI M_FindFirstFileA(LPCSTR pattern, LPWIN32_FIND_DATAA data)
{
    char full[MAX_PATH];
    if (!M_FullPath(pattern, full)) {
        return m_RealFindFirstFileA(pattern, data);
    }
    if (!M_IsUnderGame(full)) {
        return m_RealFindFirstFileA(full, data);
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

    M_AddStagedEntries(full, entries, &count, ARRAYSIZE(entries));

    WIN32_FIND_DATAA game_data;
    HANDLE game_handle = M_IsPrivate(full)
        ? INVALID_HANDLE_VALUE
        : m_RealFindFirstFileA(full, &game_data);
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

static bool M_IsDirectoryW(const wchar_t *path)
{
    const DWORD attributes = GetFileAttributesW(path);
    return attributes != INVALID_FILE_ATTRIBUTES
        && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

static bool M_IsDirectoryA(const char *path)
{
    const DWORD attributes = GetFileAttributesA(path);
    return attributes != INVALID_FILE_ATTRIBUTES
        && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

static BOOL WINAPI M_SetCurrentDirectoryA(LPCSTR path)
{
    char full[MAX_PATH];
    if (!M_FullPath(path, full)) {
        SetLastError(ERROR_PATH_NOT_FOUND);
        return FALSE;
    }
    if (!M_IsUnderGame(full) || M_IsDirectoryA(full)) {
        const BOOL result = m_RealSetCurrentDirectoryA(full);
        if (result) {
            m_VirtualDir[0] = '\0';
        }
        return result;
    }
    wchar_t out_path[MAX_PATH];
    M_MapToOut(full, out_path, MAX_PATH);
    if (!M_IsDirectoryW(out_path)) {
        SetLastError(ERROR_PATH_NOT_FOUND);
        return FALSE;
    }
    strcpy(m_VirtualDir, full);
    M_LogCall("SetCurrentDirectoryA", path, "out");
    return TRUE;
}

static DWORD WINAPI M_GetCurrentDirectoryA(DWORD size, LPSTR buffer)
{
    if (m_VirtualDir[0] == '\0') {
        return m_RealGetCurrentDirectoryA(size, buffer);
    }
    const DWORD length = (DWORD)strlen(m_VirtualDir);
    if (buffer == nullptr || size <= length) {
        return length + 1;
    }
    memcpy(buffer, m_VirtualDir, length + 1);
    return length;
}

static BOOL WINAPI M_ReadFile(
    HANDLE handle, LPVOID buffer, DWORD size, LPDWORD read,
    LPOVERLAPPED overlapped)
{
    MEMORY_HANDLE *const memory = M_FindMemory(handle);
    if (memory == nullptr) {
        return m_RealReadFile(handle, buffer, size, read, overlapped);
    }
    const DWORD left = (DWORD)memory->file->size - memory->position;
    const DWORD count = size < left ? size : left;
    memcpy(buffer, memory->file->data + memory->position, count);
    memory->position += count;
    if (read != nullptr) {
        *read = count;
    }
    return TRUE;
}

static DWORD WINAPI M_GetFileSize(HANDLE handle, LPDWORD high)
{
    MEMORY_HANDLE *const memory = M_FindMemory(handle);
    if (memory == nullptr) {
        return m_RealGetFileSize(handle, high);
    }
    if (high != nullptr) {
        *high = 0;
    }
    return (DWORD)memory->file->size;
}

static DWORD WINAPI
M_SetFilePointer(HANDLE handle, LONG distance, PLONG high, DWORD method)
{
    MEMORY_HANDLE *const memory = M_FindMemory(handle);
    if (memory == nullptr) {
        return m_RealSetFilePointer(handle, distance, high, method);
    }
    int64_t base = 0;
    if (method == FILE_CURRENT) {
        base = memory->position;
    } else if (method == FILE_END) {
        base = (int64_t)memory->file->size;
    }
    const int64_t offset = high != nullptr
        ? (int64_t)(((uint64_t)(uint32_t)*high << 32) | (uint32_t)distance)
        : distance;
    const int64_t position = base + offset;
    if (position < 0 || position > UINT32_MAX) {
        SetLastError(ERROR_NEGATIVE_SEEK);
        return INVALID_SET_FILE_POINTER;
    }
    memory->position = (DWORD)position;
    if (high != nullptr) {
        *high = 0;
    }
    return memory->position;
}

static DWORD WINAPI M_GetFileType(HANDLE handle)
{
    return M_FindMemory(handle) != nullptr ? FILE_TYPE_DISK
                                           : m_RealGetFileType(handle);
}

static BOOL WINAPI M_GetFileTime(
    HANDLE handle, LPFILETIME created, LPFILETIME accessed, LPFILETIME written)
{
    if (M_FindMemory(handle) == nullptr) {
        return m_RealGetFileTime(handle, created, accessed, written);
    }
    const FILETIME zero = {};
    if (created != nullptr) {
        *created = zero;
    }
    if (accessed != nullptr) {
        *accessed = zero;
    }
    if (written != nullptr) {
        *written = zero;
    }
    return TRUE;
}

static BOOL WINAPI M_CloseHandle(HANDLE handle)
{
    MEMORY_HANDLE *const memory = M_FindMemory(handle);
    if (memory != nullptr) {
        EnterCriticalSection(&m_Lock);
        memset(memory, 0, sizeof(*memory));
        LeaveCriticalSection(&m_Lock);
    }
    return m_RealCloseHandle(handle);
}

// Both directories are full paths without a trailing backslash. The
// private paths use backslashes; they and staged stay valid until the
// process exits. Fails when an import is missing or a staged file is past
// 4 GiB.
RESULT Overlay_Install(
    const wchar_t *const game_dir, const wchar_t *const work_dir,
    const char *const *const private_paths, const size_t count,
    const STAGED_FILE *const staged, const size_t staged_count)
{
    for (size_t i = 0; i < staged_count; i++) {
        FAIL_IF(
            staged[i].size > MAXDWORD, "staged %s is too large",
            staged[i].path);
    }
    m_PrivatePaths = private_paths;
    m_PrivateCount = count;
    m_Staged = staged;
    m_StagedCount = staged_count;
    InitializeCriticalSection(&m_Lock);
    wcsncpy(m_GameDirW, game_dir, MAX_PATH);
    m_GameDirW[MAX_PATH - 1] = L'\0';
    wcsncpy(m_WorkDirW, work_dir, MAX_PATH);
    m_WorkDirW[MAX_PATH - 1] = L'\0';
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
        { "SetCurrentDirectoryA", (uintptr_t)M_SetCurrentDirectoryA,
          (uintptr_t *)&m_RealSetCurrentDirectoryA },
        { "GetCurrentDirectoryA", (uintptr_t)M_GetCurrentDirectoryA,
          (uintptr_t *)&m_RealGetCurrentDirectoryA },
        { "ReadFile", (uintptr_t)M_ReadFile, (uintptr_t *)&m_RealReadFile },
        { "GetFileSize", (uintptr_t)M_GetFileSize,
          (uintptr_t *)&m_RealGetFileSize },
        { "SetFilePointer", (uintptr_t)M_SetFilePointer,
          (uintptr_t *)&m_RealSetFilePointer },
        { "GetFileType", (uintptr_t)M_GetFileType,
          (uintptr_t *)&m_RealGetFileType },
        { "GetFileTime", (uintptr_t)M_GetFileTime,
          (uintptr_t *)&m_RealGetFileTime },
        { "CloseHandle", (uintptr_t)M_CloseHandle,
          (uintptr_t *)&m_RealCloseHandle },
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
