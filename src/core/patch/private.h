#pragma once

#include "core/base/arena.h"
#include "core/base/hash_map.h"
#include "core/json/types.h"
#include "core/patch/stack.h"

// Holds the paths on disk of one file of a patch: under its plain name and
// under its build-specific name. Either can be nullptr.
typedef struct {
    char *plain;
    char *build;
} PATCH_FILE;

// Maps a file key to its PATCH_FILE. A key is the file's name, or for
// PATCH_LAYOUT_PATHS its path below the game directory, in lowercase and
// without the build.
typedef HASH_MAP(const char *, PATCH_FILE) PATCH_FILES;

typedef struct {
    PATCH_INFO info;
    PATCH_FILES root;
    PATCH_FILES game;
} PATCH;

struct PATCH_STACK {
    ARENA arena;
    PATCH *patches;
    int32_t count;
    PATCH_GAME game;
    char *game_id;
    bool changes_gameplay;
};

// Returns the key of the game file at path, as a new string that Mem_Free
// releases, or nullptr when path is not a valid game path.
char *PatchStack_GetGameKey(const PATCH_STACK *stack, const char *path);

// Returns whether the game file of key is one the game takes from patches:
// not a .jdiff file and not PATCH_FILE_IGNORED.
bool PatchStack_IsOverlaid(const PATCH_STACK *stack, const char *key);

// Reads the JSON file at path into a tree from allocator. Returns nullptr,
// with a log line, when the file does not read or parse.
JSON_VALUE *PatchStack_ReadJson(const char *path, const ALLOCATOR *allocator);
