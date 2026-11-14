#pragma once

#include "core/base/memory.h"
#include "core/base/result.h"
#include "core/json/types.h"
#include "core/patch/stack.h"

// Mounts th07's data files into Shell_GetFiles: th07.dat above the data
// directory, which keeps the files the archive does not have, and the
// files of thcrap patches above them. The archive finds a file by the last
// component of its path. Callers open files with Vfs_Open on
// Shell_GetFiles, VFS_STREAM for thbgm.dat.

// Describes th07 to thcrap patches: build v1.00b, the layout that keeps
// the archive's directories, and these kinds of files. ECL, STD and SHT
// files and whole MSG files change what happens in play. Of the ANM files,
// only those of the menus, the screens and the text rows are taken from
// patches: front, text, and title*, result*, music* and staff*. Every
// other file replaces the game's file.
extern const PATCH_GAME g_PatchGame;

#define FILES_ARCHIVE_COUNT 1

// Names the archive that Files_Mount mounts.
extern const char *const g_FilesArchives[FILES_ARCHIVE_COUNT];

#define FILES_DATA_COUNT 2

// Names the files that the data directory must hold, as g_GameDesc's
// data_files: th07.dat and thbgm.dat.
extern const char *const g_FilesData[FILES_DATA_COUNT];

// Mounts th07.dat of Shell_GetFiles into it at priority 11. An archive that
// is missing, or that has no th07_0100b.ver, is a failure: the original
// refuses to start without the version file of 1.00b. So is a thbgm.dat
// that is missing or is not that of 1.00, which the original refuses as
// well.
RESULT Files_Mount(void);

// Mounts the files of patches into Shell_GetFiles above the archive, so
// that a file of a patch replaces the archived file of the same name. The
// stack must stay open while Shell_GetFiles holds it. A nullptr stack
// mounts nothing.
void Files_SetPatches(const PATCH_STACK *patches);

// Returns the merged jdiff of path from the patches, from allocator, or
// nullptr when no patch has one.
JSON_VALUE *Files_LoadDiff(const char *path, const ALLOCATOR *allocator);

// Returns whether a patch replaces path or has a jdiff for it.
bool Files_IsPatched(const char *path);
