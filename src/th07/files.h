#pragma once

#include "core/base/memory.h"
#include "core/base/result.h"
#include "core/io/reader.h"
#include "core/json/types.h"
#include "core/patch/stack.h"

// Loads th07's data files: th07.dat, with the data directory below it for
// files the archive does not have, and the files of thcrap patches above
// them.

// Describes th07 to thcrap patches: build v1.00b, the layout that keeps
// the archive's directories, and these kinds of files. ECL, STD and SHT
// files and whole MSG files change what happens in play. Of the ANM files,
// only those of the menus, the screens and the text rows are taken from
// patches: front, text, and title*, result*, music* and staff*. Every
// other file replaces the game's file.
extern const PATCH_GAME g_PatchGame;

#define FILES_ARCHIVE_COUNT 1

// Names the archive that Files_Init mounts.
extern const char *const g_FilesArchives[FILES_ARCHIVE_COUNT];

// Mounts data_dir and th07.dat from it. An archive that is missing, or
// that has no th07_0100b.ver, is a failure: the original refuses to start
// without the version file of 1.00b.
RESULT Files_Init(const char *data_dir);
// Mounts the files of patches above the archive, so that a file of a
// patch replaces the archived file of the same name. The stack must stay
// open until Files_Shutdown. A nullptr stack mounts nothing.
void Files_SetPatches(const PATCH_STACK *patches);
// Releases what Files_Init mounted.
void Files_Shutdown(void);
// Opens path in memory, from the archive or else the data directory.
// The archive finds a file by the last component of its path.
RESULT Files_Load(const char *path, READER *out);
// Opens path as Files_Load does, but reads a file of the data directory
// from the disk as the reader reaches it, so it is never all in memory.
RESULT Files_Stream(const char *path, READER *out);
// Returns the merged jdiff of path from the patches, from allocator, or
// nullptr when no patch has one.
JSON_VALUE *Files_LoadDiff(const char *path, const ALLOCATOR *allocator);
// Returns whether a patch replaces path or has a jdiff for it.
bool Files_IsPatched(const char *path);
