// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "core/base/memory.h"
#include "core/base/result.h"
#include "core/io/reader.h"
#include "core/json/types.h"
#include "core/patch/stack.h"

// Loads th06's data files: the six archives, with the data directory
// below them for files no archive has, and the files of thcrap patches
// above them.

// Describes th06 to thcrap patches: build v1.02h, the flat layout, and
// these kinds of files. ECL, STD and SHT files and whole MSG files change
// what happens in play. Of the ANM files, only those of the menus, the
// screens and the text rows are taken from patches: front, text,
// replay00, and title*, select*, slpl*, result*, music* and staff*. Every
// other file replaces the game's file.
extern const PATCH_GAME g_PatchGame;

#define FILES_ARCHIVE_COUNT 6

// Names the six archives that Files_Init mounts, in that order.
extern const char *const g_FilesArchives[FILES_ARCHIVE_COUNT];

// Mounts data_dir and 紅魔郷IN, MD, ST, TL, CM and ED.DAT from it. A file
// in more than one archive comes from the earliest in that order. An
// archive that is missing, or that has no ver0102.dat, is a failure.
RESULT Files_Init(const char *data_dir);
// Mounts the files of patches above the archives, so that a file of a
// patch replaces the archived file of the same name. The stack must stay
// open until Files_Shutdown. A nullptr stack mounts nothing.
void Files_SetPatches(const PATCH_STACK *patches);
// Releases what Files_Init mounted.
void Files_Shutdown(void);
// Opens path in memory, from the archives or else the data directory.
// The archives find a file by the last component of its path.
RESULT Files_Load(const char *path, READER *out);
// Opens path as Files_Load does, but reads a file of the data directory
// from the disk as the reader reaches it, so it is never all in memory.
RESULT Files_Stream(const char *path, READER *out);
// Returns the merged jdiff of path from the patches, from allocator, or
// nullptr when no patch has one.
JSON_VALUE *Files_LoadDiff(const char *path, const ALLOCATOR *allocator);
// Returns whether a patch replaces path or has a jdiff for it.
bool Files_IsPatched(const char *path);
