// SPDX-License-Identifier: GPL-3.0-only

#include "core/base/result.h"

#include "core/base/memory.h"
#include "core/base/string.h"

#include <stdarg.h>

RESULT Result_Fail(const RESULT_SITE *const site, const char *const fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char *const message = Str_FormatV(fmt, args);
    va_end(args);
    return (RESULT) { .site = site, .message = message };
}

RESULT Result_Prefix(RESULT result, const char *const fmt, ...)
{
    if (IS_OK(result)) {
        return result;
    }
    va_list args;
    va_start(args, fmt);
    char *const context = Str_FormatV(fmt, args);
    va_end(args);

    char *const message = Str_Format("%s: %s", context, result.message);
    Mem_Free(context);
    Mem_Free(result.message);
    result.message = message;
    return result;
}

bool Result_Log(const RESULT result, const LOG_LEVEL level)
{
    if (IS_OK(result)) {
        return true;
    }
    Log_Message(
        level, result.site->file, result.site->line, "%s", result.message);
    Mem_Free(result.message);
    return false;
}

bool Result_Ignore(const RESULT result)
{
    Mem_Free(result.message);
    return IS_OK(result);
}
