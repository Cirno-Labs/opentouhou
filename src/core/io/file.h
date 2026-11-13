#pragma once

#include "core/base/array.h"
#include "core/base/memory.h"
#include "core/base/result.h"

#include <stdio.h>

// Queries the file system. Paths are UTF-8, in the platform's form; '/'
// separates directories on every platform. READER and WRITER read and write
// the files themselves. On Windows, these functions and READER and WRITER
// accept paths longer than the 260 characters of MAX_PATH.

// Holds the names of the entries of a directory. Each name is a
// NUL-terminated UTF-8 string from the allocator of the array.
typedef ARRAY(char *) FILE_LIST;

// Tells what kind of entry a path names.
typedef enum {
    FILE_TYPE_FILE,
    FILE_TYPE_DIR,
    FILE_TYPE_OTHER,
} FILE_TYPE;

// Returns whether path names an existing file, directory or other entry. A
// symbolic link counts as what it points to.
bool File_Exists(const char *path);

// Returns in *type the kind of entry at path. A symbolic link counts as what
// it points to. The query fails when path names nothing.
RESULT File_GetType(const char *path, FILE_TYPE *type);

// Creates the directory at path, and the directories above it that do not
// exist. A directory that exists already is a success.
RESULT File_MakeDir(const char *path);

// Renames the file or directory at from to to, replacing a file that is at
// to. Both paths must be on the same file system.
RESULT File_Rename(const char *from, const char *to);

// Removes the file or the empty directory at path. A path that names
// nothing is a success.
RESULT File_Remove(const char *path);

// Lists the names of the entries of a directory, without the directory and
// without "." and "..". The names are sorted byte by byte, so that the order
// is the same on every platform. The list is prepared with allocator, which
// also holds the names. On failure, the list is empty. File_FreeList
// releases the list in both cases.
RESULT File_ListDir(
    const char *path, const ALLOCATOR *allocator, FILE_LIST *list);

// Creates the file at path, or empties it, and opens it for writing as text.
// *out is a C stream that fclose closes. On failure, *out is nullptr.
RESULT File_CreateText(const char *path, FILE **out);

// Releases the names and the storage of a list, and leaves it empty.
void File_FreeList(FILE_LIST *list);
