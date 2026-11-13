#pragma once

#include "core/base/result.h"

// Runs other programs.

// Runs the program at args[0] with the arguments args, a list that ends
// with nullptr, and waits until it ends. Its standard input and standard
// error are closed. On success, output holds what the program wrote to its
// standard output, NUL-terminated, and status holds its exit status;
// Mem_Free releases output. The run fails when the program cannot start or
// its output cannot be read; output is then nullptr.
RESULT Process_Run(const char *const *args, char **output, int *status);
