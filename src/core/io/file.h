#pragma once

#include "core/base/array.h"
#include "core/base/memory.h"
#include "core/base/result.h"

// Queries the file system. Paths are UTF-8, in the platform's form; '/'
// separates directories on every platform. READER and WRITER read and write
// the files themselves.

// Holds the names of the entries of a directory. Each name is a
// NUL-terminated UTF-8 string from the allocator of the array.
typedef ARRAY(char *) FILE_LIST;

// Returns whether path names an existing file, directory or other entry. A
// symbolic link counts as what it points to.
bool File_Exists(const char *path);

// Creates the directory at path, and the directories above it that do not
// exist. A directory that exists already is a success.
RESULT File_MakeDir(const char *path);

// Lists the names of the entries of a directory, without the directory and
// without "." and "..". The names are sorted byte by byte, so that the order
// is the same on every platform. The list is prepared with allocator, which
// also holds the names. On failure, the list is empty. File_FreeList
// releases the list in both cases.
RESULT File_ListDir(
    const char *path, const ALLOCATOR *allocator, FILE_LIST *list);

// Releases the names and the storage of a list, and leaves it empty.
void File_FreeList(FILE_LIST *list);
