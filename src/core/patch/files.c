// SPDX-License-Identifier: GPL-3.0-only

#include "core/patch/files.h"

#include "core/base/hash_map.h"
#include "core/base/string.h"
#include "core/io/reader.h"
#include "core/json.h"
#include "core/patch/private.h"

#include <string.h>

typedef HASH_MAP(const char *, bool) M_KEY_SET;

typedef struct {
    VFS_SOURCE base;
    const PATCH_STACK *stack;
    const ALLOCATOR *allocator;
} M_SOURCE;

static char *M_Lowercase(const char *const text)
{
    char *const copy = Str_Dup(text);
    for (char *c = copy; *c != '\0'; c++) {
        if (*c >= 'A' && *c <= 'Z') {
            *c = (char)(*c - 'A' + 'a');
        }
    }
    return copy;
}

// Returns the disk path of the game file of key in the last patch that has
// it, or nullptr.
static const char *M_FindGameFile(
    const PATCH_STACK *const stack, const char *const key)
{
    for (int32_t i = stack->count - 1; i >= 0; i--) {
        const PATCH_FILE *const file =
            HashMap_Get(&stack->patches[i].game, key);
        if (file == nullptr) {
            continue;
        }
        if (file->build != nullptr) {
            return file->build;
        }
        if (file->plain != nullptr) {
            return file->plain;
        }
    }
    return nullptr;
}

static const char *M_FindOverlaid(
    const PATCH_STACK *const stack, const char *const path)
{
    char *const key = PatchStack_GetGameKey(stack, path);
    if (key == nullptr) {
        return nullptr;
    }
    const char *const found = PatchStack_IsOverlaid(stack, key)
        ? M_FindGameFile(stack, key)
        : nullptr;
    Mem_Free(key);
    return found;
}

static bool M_Contains(const VFS_SOURCE *const base, const char *const path)
{
    const M_SOURCE *const source = (const M_SOURCE *)base;
    return M_FindOverlaid(source->stack, path) != nullptr;
}

static RESULT M_Open(
    VFS_SOURCE *const base, const char *const path, const VFS_MODE mode,
    READER *const out)
{
    const M_SOURCE *const source = (const M_SOURCE *)base;
    const char *const disk_path = M_FindOverlaid(source->stack, path);
    FAIL_IF(disk_path == nullptr, "%s: not found in the patches", path);
    if (mode == VFS_STREAM) {
        MUST(Reader_OpenFile(out, disk_path));
        return OK;
    }
    READER file;
    MUST(Reader_OpenFile(&file, disk_path));
    const size_t size = Reader_Size(&file);
    void *const data = Reader_ReadAlloc(&file, size, source->allocator);
    const RESULT result = Reader_Result(&file);
    Reader_Close(&file);
    MUST(result, "%s", disk_path);
    Reader_OpenOwnedBuffer(out, data, size, source->allocator);
    return OK;
}

static void M_List(
    const VFS_SOURCE *const base,
    void (*const visit)(void *user, const char *path), void *const user)
{
    const M_SOURCE *const source = (const M_SOURCE *)base;
    const PATCH_STACK *const stack = source->stack;
    M_KEY_SET visited;
    HashMap_Init(&visited, &g_HeapAllocator);
    for (int32_t i = stack->count - 1; i >= 0; i--) {
        const PATCH_FILES *const files = &stack->patches[i].game;
        for (int32_t j = 0; HashMap_Next(files, &j); j++) {
            const char *const key = files->keys[j];
            bool found;
            HashMap_GetOrPut(&visited, key, &found);
            if (!found && PatchStack_IsOverlaid(stack, key)) {
                visit(user, key);
            }
        }
    }
    HashMap_Free(&visited);
}

static void M_Close(VFS_SOURCE *const base)
{
    M_SOURCE *const source = (M_SOURCE *)base;
    Allocator_Free(source->allocator, source, sizeof(*source));
}

static const VFS_SOURCE_FUNCS M_Funcs = {
    .contains = M_Contains,
    .open = M_Open,
    .list = M_List,
    .close = M_Close,
};

static void M_MergeFile(
    JSON_VALUE **const tree, const char *const path,
    const ALLOCATOR *const allocator)
{
    if (path == nullptr) {
        return;
    }
    JSON_VALUE *const value = PatchStack_ReadJson(path, allocator);
    if (value == nullptr) {
        return;
    }
    if (*tree == nullptr || (*tree)->type != JSON_OBJECT
        || value->type != JSON_OBJECT) {
        *tree = value;
        return;
    }
    Json_Merge(*tree, value, allocator);
}

VFS_SOURCE *Patch_CreateSource(
    const PATCH_STACK *const stack, const ALLOCATOR *const allocator)
{
    M_SOURCE *const source = Allocator_AllocZ(allocator, sizeof(*source));
    source->base.funcs = &M_Funcs;
    source->base.name = "patches";
    source->stack = stack;
    source->allocator = allocator;
    return &source->base;
}

JSON_VALUE *Patch_LoadJson(
    const PATCH_STACK *const stack, const PATCH_SCOPE scope,
    const char *const name, const ALLOCATOR *const allocator)
{
    if (stack == nullptr) {
        return nullptr;
    }
    char *const key = M_Lowercase(name);
    JSON_VALUE *tree = nullptr;
    for (int32_t i = 0; i < stack->count; i++) {
        const PATCH *const patch = &stack->patches[i];
        const PATCH_FILE *const file =
            HashMap_Get(scope == PATCH_ROOT ? &patch->root : &patch->game, key);
        if (file != nullptr) {
            M_MergeFile(&tree, file->plain, allocator);
            M_MergeFile(&tree, file->build, allocator);
        }
    }
    Mem_Free(key);
    return tree;
}

JSON_VALUE *Patch_LoadSettings(
    const PATCH_STACK *const stack, const ALLOCATOR *const allocator)
{
    if (stack == nullptr) {
        return nullptr;
    }
    JSON_VALUE *const global =
        Patch_LoadJson(stack, PATCH_ROOT, "global.js", allocator);
    char *const name = Str_Format("%s.js", stack->game_id);
    JSON_VALUE *const game = Patch_LoadJson(stack, PATCH_ROOT, name, allocator);
    Mem_Free(name);
    if (global == nullptr || global->type != JSON_OBJECT) {
        return game != nullptr ? game : global;
    }
    Json_Merge(global, game, allocator);
    return global;
}

JSON_VALUE *Patch_LoadDiff(
    const PATCH_STACK *const stack, const char *const path,
    const ALLOCATOR *const allocator)
{
    if (stack == nullptr) {
        return nullptr;
    }
    char *const key = PatchStack_GetGameKey(stack, path);
    if (key == nullptr) {
        return nullptr;
    }
    char *const name = Str_Format("%s.jdiff", key);
    JSON_VALUE *const tree =
        Patch_LoadJson(stack, PATCH_GAME_DIR, name, allocator);
    Mem_Free(name);
    Mem_Free(key);
    return tree;
}

bool Patch_Touches(const PATCH_STACK *const stack, const char *const path)
{
    if (stack == nullptr) {
        return false;
    }
    char *const key = PatchStack_GetGameKey(stack, path);
    if (key == nullptr) {
        return false;
    }
    char *const diff = Str_Format("%s.jdiff", key);
    const bool touches = (PatchStack_IsOverlaid(stack, key)
                          && M_FindGameFile(stack, key) != nullptr)
        || M_FindGameFile(stack, diff) != nullptr;
    Mem_Free(diff);
    Mem_Free(key);
    return touches;
}
