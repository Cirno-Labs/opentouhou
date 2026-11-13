#pragma once

#include <stdio.h>

// Tells what out of Main_Run is. On a terminal, progress bars redraw in
// place with '\r'; elsewhere they print only their last state. An ANSI
// terminal also shows the files that download under the bar of their
// patch, and redraws them with escape sequences.
typedef enum {
    MAIN_OUTPUT_FILE,
    MAIN_OUTPUT_TERMINAL,
    MAIN_OUTPUT_ANSI,
} MAIN_OUTPUT;

// Runs othpatch with UTF-8 arguments, writes its messages to out and
// returns its exit code: 0 on success, 1 on failure. --help prints what the
// arguments do. exe_dir is the folder of the executable, or nullptr when the
// platform cannot tell it. in answers the questions before an update, or is
// nullptr to ask none. output tells what out is.
int Main_Run(
    int argc, char **argv, const char *version, const char *exe_dir, FILE *in,
    FILE *out, MAIN_OUTPUT output);
