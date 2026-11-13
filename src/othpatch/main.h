#pragma once

#include <stdio.h>

// Runs othpatch with UTF-8 arguments, writes its messages to out and
// returns its exit code: 0 on success, 1 on failure. The entry point of
// each platform calls it with OTHPATCH_VERSION as version, with
// Path_GetExeDir() as exe_dir, and with stdin as in when stdin is a
// terminal, or else nullptr.
//
// othpatch downloads the files that every game shares, and the files of
// each game it supports that is present: a game named with --game, a
// game whose executable is in exe_dir (o<game>, o<game>.exe,
// o<game>.AppImage or the o<game>.app bundle), and a game that a patch in
// the patches folder has a <game>/ folder for. Without a game, the run
// fails. A portable.txt in exe_dir puts the patches folder in exe_dir.
// exe_dir can be nullptr when the platform cannot tell it.
//
// Then othpatch checks each game in exe_dir in the form of the platform's
// packages for a newer release, and then itself, in the release index at
// --releases (othpatch/release.h). It installs a newer release with --update,
// or when the answer to a question on in is yes. Without in, it only says that
// a newer release exists. A check that fails, such as without a network, prints
// one line and does not change the exit code; an install that fails does.
// A game or an othpatch whose version is not of the form "X.Y.Z" is a
// development build and is never updated. A game package also carries an
// othpatch, which is installed only when it is newer than the othpatch in
// exe_dir.
int Main_Run(
    int argc, char **argv, const char *version, const char *exe_dir, FILE *in,
    FILE *out);
