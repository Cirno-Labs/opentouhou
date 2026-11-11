#pragma once

#include <stdio.h>

// Runs othpatch with UTF-8 arguments, writes its messages to out and
// returns its exit code: 0 on success, 1 on failure. The entry point of
// each platform calls it.
int Main_Run(int argc, char **argv, FILE *out);
