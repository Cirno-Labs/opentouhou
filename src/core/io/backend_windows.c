// SPDX-License-Identifier: GPL-3.0-only

#include "core/io/backend.h"

#include "core/base/memory.h"
#include "core/base/string.h"

#include <string.h>
#include <windows.h>

static wchar_t *M_ToWide(const char *const text)
{
    const int size = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, nullptr, 0);
    if (size == 0) {
        return nullptr;
    }
    wchar_t *const wide = Mem_AllocArr((size_t)size, sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, wide, size);
    return wide;
}

static char *M_ToUtf8(const wchar_t *const wide)
{
    const int size = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, wide, -1, nullptr, 0, nullptr, nullptr);
    if (size == 0) {
        return nullptr;
    }
    char *const text = Mem_Alloc((size_t)size);
    WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, wide, -1, text, size, nullptr, nullptr);
    return text;
}

// Returns the absolute form of path, with '\' as the separator and without
// "." and "..", or nullptr when Windows cannot make one.
static char *M_FullPath(const char *const path)
{
    wchar_t *const wide = M_ToWide(path);
    if (wide == nullptr) {
        return nullptr;
    }
    const DWORD size = GetFullPathNameW(wide, 0, nullptr, nullptr);
    char *full = nullptr;
    if (size != 0) {
        wchar_t *const buffer = Mem_AllocArr(size, sizeof(wchar_t));
        const DWORD length = GetFullPathNameW(wide, size, buffer, nullptr);
        if (length != 0 && length < size) {
            full = M_ToUtf8(buffer);
        }
        Mem_Free(buffer);
    }
    Mem_Free(wide);
    return full;
}

char *IoBackend_SystemPath(const char *const path)
{
    if (path[0] == '\0' || Str_StartsWith(path, "\\\\?\\")
        || Str_StartsWith(path, "\\\\.\\")) {
        return Str_Dup(path);
    }
    char *const full = M_FullPath(path);
    if (full == nullptr) {
        return Str_Dup(path);
    }
    // Removes the separators at the end, except the one of a drive root such
    // as "C:\", because "\\?\C:" names the volume and not its root
    // directory.
    size_t length = strlen(full);
    while (length > 3 && full[length - 1] == '\\') {
        full[--length] = '\0';
    }
    if (Str_StartsWith(full, "\\\\.\\")) {
        return full;
    }
    char *const system_path = Str_StartsWith(full, "\\\\")
        ? Str_Format("\\\\?\\UNC\\%s", full + 2)
        : Str_Format("\\\\?\\%s", full);
    Mem_Free(full);
    return system_path;
}
