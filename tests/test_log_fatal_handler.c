#include "core/base/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Ends the test with success, because Log_Fatal aborts when the handler
// returns.
static void M_Handle(
    const char *const message, const char *const file, int line)
{
    if (strcmp(message, "Stopped at 7") != 0
        || strcmp(file, "test_log_fatal_handler.c") != 0 || line <= 0) {
        fprintf(stderr, "unexpected fatal: %s at %s:%d\n", message, file, line);
        return;
    }
    exit(EXIT_SUCCESS);
}

int main(void)
{
    Log_SetFatalHandler(M_Handle);
    LOG_FATAL("Stopped at %d", 7);
}
