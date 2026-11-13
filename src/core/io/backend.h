#pragma once

#include <stdio.h>

// Holds the parts of core/io that differ between platforms.

// Returns path in the form that the platform's file functions take, as a new
// string that Mem_Free releases. On Windows, this is the extended form
// "\\?\C:\dir\file", which the file functions accept beyond the 260
// characters of MAX_PATH. The extended form is absolute and has no "." or
// "..", so the path is first made absolute against the current directory.
// Elsewhere, the path is unchanged.
char *IoBackend_SystemPath(const char *path);

// Creates the file at system_path, a path from IoBackend_SystemPath, or
// empties it, and opens it for writing as text. Returns nullptr and sets
// errno on failure.
FILE *IoBackend_CreateText(const char *system_path);
