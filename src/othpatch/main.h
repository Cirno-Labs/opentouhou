#pragma once

#include <stdio.h>

// Runs othpatch with UTF-8 arguments, writes its messages to out and
// returns its exit code: 0 on success, 1 on failure. --help prints what the
// arguments do. exe_dir is the folder of the executable, or nullptr when the
// platform cannot tell it. in answers the questions before an update, or is
// nullptr to ask none. terminal tells whether out is a terminal: progress
// bars then redraw in place with '\r', and otherwise print only their last
// state.
int Main_Run(
    int argc, char **argv, const char *version, const char *exe_dir, FILE *in,
    FILE *out, bool terminal);
