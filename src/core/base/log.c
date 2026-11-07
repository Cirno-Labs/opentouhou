#include "core/base/log.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#define M_BUFFER_SIZE 1024

static LOG_LEVEL m_MinLevel = LOG_LEVEL_INFO;

static const char *const m_LevelNames[] = {
    [LOG_LEVEL_DEBUG] = "DEBUG",
    [LOG_LEVEL_INFO] = "INFO",
    [LOG_LEVEL_WARNING] = "WARNING",
    [LOG_LEVEL_ERROR] = "ERROR",
};

static const char *M_GetFileName(const char *const path)
{
    const char *name = path;
    for (const char *c = path; *c != '\0'; c++) {
        if (*c == '/' || *c == '\\') {
            name = c + 1;
        }
    }
    return name;
}

static void M_Write(
    const char *const level_name, const char *const file, const int line,
    const char *const fmt, va_list args)
{
    char buffer[M_BUFFER_SIZE];
    char *message = buffer;

    va_list measure_args;
    va_copy(measure_args, args);
    const int length = vsnprintf(buffer, sizeof(buffer), fmt, measure_args);
    va_end(measure_args);

    // The log cannot use core/base/memory, which logs its own failures.
    if (length >= M_BUFFER_SIZE) {
        char *const large = malloc((size_t)length + 1);
        if (large != nullptr) {
            vsnprintf(large, (size_t)length + 1, fmt, args);
            message = large;
        }
    }

    fprintf(
        stderr, "%s %s:%d: %s\n", level_name, M_GetFileName(file), line,
        length < 0 ? fmt : message);
    if (message != buffer) {
        free(message);
    }
}

void Log_SetMinLevel(const LOG_LEVEL level)
{
    m_MinLevel = level;
}

LOG_LEVEL Log_GetMinLevel(void)
{
    return m_MinLevel;
}

void Log_Message(
    const LOG_LEVEL level, const char *const file, const int line,
    const char *const fmt, ...)
{
    if (level < m_MinLevel) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    M_Write(m_LevelNames[level], file, line, fmt, args);
    va_end(args);
}

void Log_Fatal(
    const char *const file, const int line, const char *const fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    M_Write("FATAL", file, line, fmt, args);
    va_end(args);
    fflush(stderr);
    abort();
}
