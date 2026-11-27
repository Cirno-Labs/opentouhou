#pragma once

// Gives each test case an empty directory of its own. The directories are
// made in the directory that meson passes in TEST_DIR, which is inside the
// build directory, so that no test touches a file anywhere else.

#include <stddef.h>

// Returns the path of an empty directory for the test case name. Mem_Free
// releases the path. The runs that meson test --repeat makes after the
// first, which may run at the same time, each get a directory of their own.
char *TestDir_Prepare(const char *name);

// Returns directory/name. Mem_Free releases the path.
char *TestDir_Join(const char *directory, const char *name);

// Returns the file:// URL of the absolute path: "file://" and the path, or
// "file:///" and the path for a Windows path with a drive, such as
// "C:/dir". Mem_Free releases the URL.
char *TestDir_FileUrl(const char *path);

// Writes size bytes of data to the file at path, and stops the test when it
// cannot.
void TestDir_Write(const char *path, const void *data, size_t size);

// Returns the contents of the file at path with a NUL after them, or nullptr
// when the file cannot be read. Mem_Free releases them.
char *TestDir_Read(const char *path);
