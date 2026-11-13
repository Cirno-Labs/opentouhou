// SPDX-License-Identifier: GPL-3.0-only

#include "othpatch/fetch/file.h"

#include "othpatch/fetch.h"

#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/io/reader.h"

#include <string.h>

static int M_HexValue(const char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

static bool M_IsDriveLetter(const char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

// Returns the path of a file:// URL, decoded, or nullptr when the URL is
// not one. Mem_Free releases the path.
static char *M_DecodePath(const char *const url)
{
    if (!Str_StartsWith(url, "file://")) {
        return nullptr;
    }
    const char *start = url + strlen("file://");
    if (Str_StartsWith(start, "localhost/")) {
        start += strlen("localhost");
    }
    if (start[0] != '/') {
        return nullptr;
    }
    const size_t length = strcspn(start, "?#");
    char *const path = Mem_Alloc(length + 1);
    size_t size = 0;
    for (size_t i = 0; i < length; i++) {
        if (start[i] == '%') {
            const int high = i + 2 < length ? M_HexValue(start[i + 1]) : -1;
            const int low = high < 0 ? -1 : M_HexValue(start[i + 2]);
            if (low < 0 || (high == 0 && low == 0)) {
                Mem_Free(path);
                return nullptr;
            }
            path[size++] = (char)(high * 16 + low);
            i += 2;
        } else {
            path[size++] = start[i];
        }
    }
    path[size] = '\0';
    if (M_IsDriveLetter(path[1]) && path[2] == ':') {
        memmove(path, path + 1, size);
    }
    return path;
}

RESULT FetchFile_Read(
    const char *const url, uint8_t **const data, size_t *const size)
{
    *data = nullptr;
    *size = 0;
    char *const path = M_DecodePath(url);
    FAIL_IF(path == nullptr, "%s is not a valid file URL", url);
    READER reader;
    const RESULT open = Reader_OpenFile(&reader, path);
    Mem_Free(path);
    MUST(open);
    const size_t file_size = Reader_Size(&reader);
    if (file_size > FETCH_MAX_SIZE) {
        Reader_Close(&reader);
        return FAIL("%s is larger than %zu bytes", url, FETCH_MAX_SIZE);
    }
    uint8_t *const contents =
        Reader_ReadAlloc(&reader, file_size, &g_HeapAllocator);
    Reader_Close(&reader);
    MUST(Reader_Result(&reader));
    *data = contents != nullptr ? contents : Mem_Alloc(0);
    *size = file_size;
    return OK;
}
