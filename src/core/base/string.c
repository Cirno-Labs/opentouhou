#include "core/base/string.h"

#include "core/base/log.h"
#include "core/base/memory.h"

#include <stdio.h>
#include <string.h>

static char M_ToLowerAscii(const char c)
{
    return c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c;
}

char *Str_Dup(const char *const text)
{
    return Mem_Dup(text, strlen(text) + 1);
}

char *Str_Format(const char *const fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char *const result = Str_FormatV(fmt, args);
    va_end(args);
    return result;
}

char *Str_FormatV(const char *const fmt, va_list args)
{
    va_list measure_args;
    va_copy(measure_args, args);
    const int length = vsnprintf(nullptr, 0, fmt, measure_args);
    va_end(measure_args);
    if (length < 0) {
        LOG_FATAL("Invalid format string: %s", fmt);
    }

    char *const result = Mem_Alloc((size_t)length + 1);
    vsnprintf(result, (size_t)length + 1, fmt, args);
    return result;
}

bool Str_StartsWith(const char *const text, const char *const prefix)
{
    return strncmp(text, prefix, strlen(prefix)) == 0;
}

bool Str_EndsWith(const char *const text, const char *const suffix)
{
    const size_t text_length = strlen(text);
    const size_t suffix_length = strlen(suffix);
    return suffix_length <= text_length
        && memcmp(text + text_length - suffix_length, suffix, suffix_length)
        == 0;
}

bool Str_EqualIgnoreCase(const char *a, const char *b)
{
    while (*a != '\0' && M_ToLowerAscii(*a) == M_ToLowerAscii(*b)) {
        a++;
        b++;
    }
    return M_ToLowerAscii(*a) == M_ToLowerAscii(*b);
}
