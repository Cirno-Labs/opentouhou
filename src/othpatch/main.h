#pragma once

#include <stdio.h>

// Runs othpatch with UTF-8 arguments, writes its messages to out and
// returns its exit code: 0 on success, 1 on failure. The entry point of
// each platform calls it with Path_GetExeDir() as exe_dir.
//
// othpatch downloads the files that every game shares, and the files of
// each game it supports that is present: a game named with --game, a
// game whose executable is in exe_dir (o<game>, o<game>.exe,
// o<game>.AppImage or the o<game>.app bundle), and a game that a patch in
// the patches folder has a <game>/ folder for. Without a game, the run
// fails. A portable.txt in exe_dir puts the patches folder in exe_dir.
// exe_dir can be nullptr when the platform cannot tell it.
int Main_Run(int argc, char **argv, const char *exe_dir, FILE *out);
