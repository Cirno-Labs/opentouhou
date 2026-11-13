// SPDX-License-Identifier: GPL-3.0-only

#include "core/vfs/normalize.h"

#include <string.h>

static bool M_IsAsciiUpper(const char c)
{
    return c >= 'A' && c <= 'Z';
}

static bool M_IsAsciiLower(const char c)
{
    return c >= 'a' && c <= 'z';
}

static bool M_IsAbsolute(const char *const buffer, const size_t length)
{
    return length >= 2 && M_IsAsciiLower(buffer[0]) && buffer[1] == ':';
}

RESULT VfsNormalize_Path(
    const char *const path, const ALLOCATOR *const allocator, char **const out)
{
    *out = nullptr;
    const size_t length = strlen(path);
    char *const buffer = Allocator_Alloc(allocator, length + 1);
    for (size_t i = 0; i < length; i++) {
        const char c = path[i];
        if (c == '\\') {
            buffer[i] = '/';
        } else if (M_IsAsciiUpper(c)) {
            buffer[i] = (char)(c - 'A' + 'a');
        } else {
            buffer[i] = c;
        }
    }
    buffer[length] = '\0';

    if (M_IsAbsolute(buffer, length)) {
        const RESULT result = FAIL("%s: an absolute path is not allowed", path);
        Allocator_Free(allocator, buffer, length + 1);
        return result;
    }

    char *write = buffer;
    const char *read = buffer;
    while (*read != '\0') {
        while (*read == '/') {
            read++;
        }
        const char *const component = read;
        while (*read != '\0' && *read != '/') {
            read++;
        }
        const size_t component_length = (size_t)(read - component);
        if (component_length == 2 && component[0] == '.'
            && component[1] == '.') {
            const RESULT result =
                FAIL("%s: a \"..\" component is not allowed", path);
            Allocator_Free(allocator, buffer, length + 1);
            return result;
        }
        if (component_length > 0) {
            if (write != buffer) {
                *write++ = '/';
            }
            memmove(write, component, component_length);
            write += component_length;
        }
    }
    *write = '\0';
    *out = buffer;
    return OK;
}
