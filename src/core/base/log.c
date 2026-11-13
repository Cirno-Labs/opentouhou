#include "core/base/log.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#define M_BUFFER_SIZE 1024

static LOG_LEVEL m_MinLevel = LOG_LEVEL_INFO;
static LOG_FATAL_HANDLER m_FatalHandler = nullptr;
static FILE *m_File = nullptr;

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

// Formats a message into buffer, or into a new heap block when buffer is
// too small. A format that fails gives fmt unchanged.
static char *M_Format(char *const buffer, const char *const fmt, va_list args)
{
    va_list measure_args;
    va_copy(measure_args, args);
    const int length = vsnprintf(buffer, M_BUFFER_SIZE, fmt, measure_args);
    va_end(measure_args);
    if (length < 0) {
        snprintf(buffer, M_BUFFER_SIZE, "%s", fmt);
        return buffer;
    }
    // The log cannot use core/base/memory, which logs its own failures.
    if (length >= M_BUFFER_SIZE) {
        char *const large = malloc((size_t)length + 1);
        if (large != nullptr) {
            vsnprintf(large, (size_t)length + 1, fmt, args);
            return large;
        }
    }
    return buffer;
}

static void M_Write(
    const char *const level_name, const char *const file, const int line,
    const char *const message)
{
    const char *const name = M_GetFileName(file);
    fprintf(stderr, "%s %s:%d: %s\n", level_name, name, line, message);
    if (m_File != nullptr) {
        fprintf(m_File, "%s %s:%d: %s\n", level_name, name, line, message);
        fflush(m_File);
    }
}

void Log_SetFatalHandler(const LOG_FATAL_HANDLER handler)
{
    m_FatalHandler = handler;
}

void Log_SetFile(FILE *const file)
{
    m_File = file;
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
    char buffer[M_BUFFER_SIZE];
    va_list args;
    va_start(args, fmt);
    char *const message = M_Format(buffer, fmt, args);
    va_end(args);
    M_Write(m_LevelNames[level], file, line, message);
    if (message != buffer) {
        free(message);
    }
}

void Log_Fatal(
    const char *const file, const int line, const char *const fmt, ...)
{
    char buffer[M_BUFFER_SIZE];
    va_list args;
    va_start(args, fmt);
    char *const message = M_Format(buffer, fmt, args);
    va_end(args);
    M_Write("FATAL", file, line, message);
    fflush(stderr);
    const LOG_FATAL_HANDLER handler = m_FatalHandler;
    m_FatalHandler = nullptr;
    if (handler != nullptr) {
        handler(message, M_GetFileName(file), line);
    }
    abort();
}
