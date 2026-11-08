#include "workdir.h"

#include <stdint.h>
#include <wchar.h>

RESULT Workdir_CreateTemp(
    const wchar_t *game_id, wchar_t *out_dir, size_t out_dir_capacity)
{
    wchar_t temp_path[MAX_PATH];
    if (GetTempPathW(ARRAYSIZE(temp_path), temp_path) == 0) {
        return FAIL("GetTempPathW failed: %lu", (unsigned long)GetLastError());
    }

    for (int attempt = 0; attempt < 100; attempt++) {
        wchar_t candidate[MAX_PATH];
        swprintf(
            candidate, ARRAYSIZE(candidate), L"%ls%ls-capture-%lu-%d",
            temp_path, game_id, (unsigned long)GetCurrentProcessId(), attempt);
        if (CreateDirectoryW(candidate, nullptr)) {
            wcsncpy(out_dir, candidate, out_dir_capacity);
            out_dir[out_dir_capacity - 1] = L'\0';
            return OK;
        }
    }
    return FAIL("could not create a unique temporary directory");
}

static void M_RemoveTree(const wchar_t *dir)
{
    wchar_t pattern[MAX_PATH];
    swprintf(pattern, ARRAYSIZE(pattern), L"%ls\\*", dir);

    WIN32_FIND_DATAW data;
    HANDLE find = FindFirstFileW(pattern, &data);
    if (find == INVALID_HANDLE_VALUE) {
        RemoveDirectoryW(dir);
        return;
    }

    do {
        if (wcscmp(data.cFileName, L".") == 0
            || wcscmp(data.cFileName, L"..") == 0) {
            continue;
        }
        wchar_t child[MAX_PATH];
        swprintf(child, ARRAYSIZE(child), L"%ls\\%ls", dir, data.cFileName);
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            M_RemoveTree(child);
        } else {
            DeleteFileW(child);
        }
    } while (FindNextFileW(find, &data));
    FindClose(find);

    RemoveDirectoryW(dir);
}

void Workdir_RemoveTemp(const wchar_t *dir)
{
    M_RemoveTree(dir);
}
