// SPDX-License-Identifier: GPL-3.0-only

#include "core/base/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE *m_File;

// Ends the test with success when the fatal line is in the log file,
// because Log_Fatal aborts when the handler returns.
static void M_Handle(
    const char *const message, const char *const file, int line)
{
    if (strcmp(message, "Stopped at 7") != 0
        || strcmp(file, "test_log_fatal_handler.c") != 0 || line <= 0) {
        fprintf(stderr, "unexpected fatal: %s at %s:%d\n", message, file, line);
        return;
    }
    char text[256];
    rewind(m_File);
    const size_t size = fread(text, 1, sizeof(text) - 1, m_File);
    text[size] = '\0';
    if (strstr(text, "FATAL test_log_fatal_handler.c:") != text
        || strstr(text, ": Stopped at 7\n") == nullptr) {
        fprintf(stderr, "unexpected log file: %s\n", text);
        return;
    }
    exit(EXIT_SUCCESS);
}

int main(void)
{
    m_File = fopen("test_log_fatal_handler.txt", "w+");
    if (m_File == nullptr) {
        return EXIT_FAILURE;
    }
    Log_SetFile(m_File);
    Log_SetFatalHandler(M_Handle);
    LOG_FATAL("Stopped at %d", 7);
}
