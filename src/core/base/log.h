#pragma once

// Writes messages to the standard error stream, one line each, with the
// level and the source position of the message.

typedef enum {
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_ERROR,
} LOG_LEVEL;

#define LOG_DEBUG(...)                                                         \
    Log_Message(LOG_LEVEL_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...)                                                          \
    Log_Message(LOG_LEVEL_INFO, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARNING(...)                                                       \
    Log_Message(LOG_LEVEL_WARNING, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...)                                                         \
    Log_Message(LOG_LEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__)

// Logs a message and aborts the program.
#define LOG_FATAL(...) Log_Fatal(__FILE__, __LINE__, __VA_ARGS__)

// Sets the lowest level that is written. Messages below it are dropped. The
// default is LOG_LEVEL_INFO.
void Log_SetMinLevel(LOG_LEVEL level);
LOG_LEVEL Log_GetMinLevel(void);

// Writes one message with a printf-style format. The LOG_ macros call it with
// the source position.
[[gnu::format(printf, 4, 5)]]
void Log_Message(
    LOG_LEVEL level, const char *file, int line, const char *fmt, ...);

// Writes one message regardless of the minimum level and aborts the program.
[[noreturn, gnu::format(printf, 3, 4)]]
void Log_Fatal(const char *file, int line, const char *fmt, ...);
