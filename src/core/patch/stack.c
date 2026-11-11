#include "core/patch/private.h"

#include "core/base/array.h"
#include "core/base/assert.h"
#include "core/base/log.h"
#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/io/file.h"
#include "core/io/path.h"
#include "core/io/reader.h"
#include "core/json.h"
#include "core/vfs/normalize.h"

#include <SDL3/SDL.h>
#include <stdlib.h>
#include <string.h>

// Sets how much address space the stack reserves for the patch.js trees.
#define M_ARENA_CAPACITY (64 * 1024 * 1024)
// Caps how deep a game directory may nest, so that a symbolic link cycle
// cannot walk forever.
#define M_MAX_DEPTH 32

typedef ARRAY(int32_t) M_INDICES;

typedef struct {
    char *id;
    const char *name; // the part of the id after the '/'
    char *dir;
    const JSON_VALUE *patch_js;
    M_INDICES dependencies;
    bool excluded;
    bool placed;
} M_CANDIDATE;

typedef ARRAY(M_CANDIDATE) M_CANDIDATES;

static void M_ToLower(char *const text)
{
    for (char *c = text; *c != '\0'; c++) {
        if (*c >= 'A' && *c <= 'Z') {
            *c = (char)(*c - 'A' + 'a');
        }
    }
}

// Returns the key of a file name: the name in lowercase, without the
// build after its first part. *is_build tells whether the name had it.
static char *M_KeyOfName(
    const char *const name, const char *const build, bool *const is_build)
{
    char *const key = Str_Dup(name);
    M_ToLower(key);
    *is_build = false;
    char *const dot = strchr(key, '.');
    const size_t build_length = strlen(build);
    if (dot != nullptr && build_length > 0
        && strncmp(dot + 1, build, build_length) == 0
        && (dot[1 + build_length] == '.' || dot[1 + build_length] == '\0')) {
        memmove(
            dot, dot + 1 + build_length, strlen(dot + 1 + build_length) + 1);
        *is_build = true;
    }
    return key;
}

static void M_Record(
    PATCH_FILES *const files, const char *const key, char *const disk_path,
    const bool is_build)
{
    PATCH_FILE *const file = HashMap_GetOrPut(files, key);
    char **const slot = is_build ? &file->build : &file->plain;
    Mem_Free(*slot);
    *slot = disk_path;
}

static void M_IndexDir(
    PATCH_FILES *const files, const char *const disk_dir,
    const char *const prefix, const char *const build, const bool recursive,
    const int depth)
{
    FILE_LIST list;
    if (depth > M_MAX_DEPTH
        || !Result_Ignore(File_ListDir(disk_dir, &g_HeapAllocator, &list))) {
        return;
    }
    for (int32_t i = 0; i < list.count; i++) {
        const char *const name = list.items[i];
        char *const disk_path = Path_Join(&g_HeapAllocator, disk_dir, name);
        SDL_PathInfo info;
        if (!SDL_GetPathInfo(disk_path, &info)) {
            Mem_Free(disk_path);
            continue;
        }
        if (info.type == SDL_PATHTYPE_DIRECTORY) {
            if (recursive) {
                char *const sub_prefix = Str_Format("%s%s/", prefix, name);
                M_ToLower(sub_prefix);
                M_IndexDir(
                    files, disk_path, sub_prefix, build, true, depth + 1);
                Mem_Free(sub_prefix);
            }
            Mem_Free(disk_path);
            continue;
        }
        bool is_build;
        char *const name_key = M_KeyOfName(name, build, &is_build);
        char *const key = Str_Format("%s%s", prefix, name_key);
        M_Record(files, key, disk_path, is_build);
        Mem_Free(key);
        Mem_Free(name_key);
    }
    File_FreeList(&list);
}

static void M_FreeFiles(PATCH_FILES *const files)
{
    for (int32_t i = 0; HashMap_Next(files, &i); i++) {
        Mem_Free(files->values[i].plain);
        Mem_Free(files->values[i].build);
    }
    HashMap_Free(files);
}

static int M_CompareCandidates(const void *const a, const void *const b)
{
    return strcmp(((const M_CANDIDATE *)a)->id, ((const M_CANDIDATE *)b)->id);
}

static void M_AddCandidate(
    PATCH_STACK *const stack, M_CANDIDATES *const candidates,
    const char *const dir, const char *const repo, const char *const name)
{
    char *const repo_dir = Path_Join(&g_HeapAllocator, dir, repo);
    char *const patch_dir = Path_Join(&g_HeapAllocator, repo_dir, name);
    char *const patch_js_path =
        Path_Join(&g_HeapAllocator, patch_dir, "patch.js");
    Mem_Free(repo_dir);
    if (!File_Exists(patch_js_path)) {
        Mem_Free(patch_js_path);
        Mem_Free(patch_dir);
        return;
    }
    M_CANDIDATE candidate = {
        .id = Str_Format("%s/%s", repo, name),
        .dir = Str_Format("%s/", patch_dir),
    };
    candidate.name = candidate.id + strlen(repo) + 1;
    Array_Init(&candidate.dependencies, &g_HeapAllocator);
    candidate.patch_js =
        PatchStack_ReadJson(patch_js_path, Arena_Allocator(&stack->arena));
    if (candidate.patch_js != nullptr
        && candidate.patch_js->type != JSON_OBJECT) {
        LOG_WARNING("%s: not an object", patch_js_path);
        candidate.patch_js = nullptr;
    }
    if (candidate.patch_js == nullptr) {
        LOG_WARNING("Left out the patch %s: patch.js is broken", candidate.id);
        candidate.excluded = true;
    }
    Array_Append(candidates, candidate);
    Mem_Free(patch_js_path);
    Mem_Free(patch_dir);
}

static RESULT M_FindCandidates(
    PATCH_STACK *const stack, const char *const dir,
    M_CANDIDATES *const candidates)
{
    FILE_LIST repos;
    MUST(File_ListDir(dir, &g_HeapAllocator, &repos));
    for (int32_t i = 0; i < repos.count; i++) {
        char *const repo_dir = Path_Join(&g_HeapAllocator, dir, repos.items[i]);
        FILE_LIST patches;
        if (Result_Ignore(File_ListDir(repo_dir, &g_HeapAllocator, &patches))) {
            for (int32_t j = 0; j < patches.count; j++) {
                M_AddCandidate(
                    stack, candidates, dir, repos.items[i], patches.items[j]);
            }
            File_FreeList(&patches);
        }
        Mem_Free(repo_dir);
    }
    File_FreeList(&repos);
    if (candidates->count > 1) {
        qsort(
            candidates->items, (size_t)candidates->count, sizeof(M_CANDIDATE),
            M_CompareCandidates);
    }
    return OK;
}

static int32_t M_FindId(
    const M_CANDIDATES *const candidates, const char *const id)
{
    for (int32_t i = 0; i < candidates->count; i++) {
        if (strcmp(candidates->items[i].id, id) == 0) {
            return i;
        }
    }
    return -1;
}

// Returns the candidate that a dependency of candidate names, or -1.
static int32_t M_ResolveDependency(
    const M_CANDIDATES *const candidates, const M_CANDIDATE *const candidate,
    const char *const dependency)
{
    if (strchr(dependency, '/') != nullptr) {
        return M_FindId(candidates, dependency);
    }
    const size_t repo_length = (size_t)(candidate->name - candidate->id);
    char *const same_repo =
        Str_Format("%.*s%s", (int)repo_length, candidate->id, dependency);
    const int32_t found = M_FindId(candidates, same_repo);
    Mem_Free(same_repo);
    if (found >= 0) {
        return found;
    }
    for (int32_t i = 0; i < candidates->count; i++) {
        if (strcmp(candidates->items[i].name, dependency) == 0) {
            return i;
        }
    }
    return -1;
}

static bool M_SupportsGame(
    const M_CANDIDATE *const candidate, const char *const game_id)
{
    const JSON_VALUE *const games =
        Json_Get(candidate->patch_js, "supported_games");
    if (games == nullptr || games->type != JSON_ARRAY) {
        return true;
    }
    for (int32_t i = 0; i < games->array.count; i++) {
        const char *const game = Json_GetString(Json_At(games, i), "");
        if (strcmp(game, game_id) == 0) {
            return true;
        }
    }
    return false;
}

static bool M_IsDisabled(
    const char *const id, const char *const *const disabled,
    const int32_t disabled_count)
{
    for (int32_t i = 0; i < disabled_count; i++) {
        if (strcmp(disabled[i], id) == 0) {
            return true;
        }
    }
    return false;
}

static void M_Exclude(
    M_CANDIDATES *const candidates, const char *const game_id,
    const char *const *const disabled, const int32_t disabled_count)
{
    for (int32_t i = 0; i < candidates->count; i++) {
        M_CANDIDATE *const candidate = &candidates->items[i];
        if (candidate->excluded) {
            continue;
        }
        if (M_IsDisabled(candidate->id, disabled, disabled_count)) {
            LOG_INFO("Left out the patch %s: disabled", candidate->id);
            candidate->excluded = true;
            continue;
        }
        if (!M_SupportsGame(candidate, game_id)) {
            LOG_INFO(
                "Left out the patch %s: it does not support %s", candidate->id,
                game_id);
            candidate->excluded = true;
            continue;
        }
        const JSON_VALUE *const dependencies =
            Json_Get(candidate->patch_js, "dependencies");
        const int32_t count =
            dependencies != nullptr && dependencies->type == JSON_ARRAY
            ? dependencies->array.count
            : 0;
        for (int32_t j = 0; j < count; j++) {
            const char *const dependency =
                Json_GetString(Json_At(dependencies, j), nullptr);
            if (dependency == nullptr) {
                continue;
            }
            const int32_t found =
                M_ResolveDependency(candidates, candidate, dependency);
            if (found < 0) {
                LOG_WARNING(
                    "Left out the patch %s: it needs %s, which is missing",
                    candidate->id, dependency);
                candidate->excluded = true;
                break;
            }
            Array_Append(&candidate->dependencies, found);
        }
    }

    bool changed = true;
    while (changed) {
        changed = false;
        for (int32_t i = 0; i < candidates->count; i++) {
            M_CANDIDATE *const candidate = &candidates->items[i];
            for (int32_t j = 0;
                 !candidate->excluded && j < candidate->dependencies.count;
                 j++) {
                const M_CANDIDATE *const dependency =
                    &candidates->items[candidate->dependencies.items[j]];
                if (dependency->excluded) {
                    LOG_WARNING(
                        "Left out the patch %s: it needs %s, which is left "
                        "out",
                        candidate->id, dependency->id);
                    candidate->excluded = true;
                    changed = true;
                }
            }
        }
    }
}

static bool M_IsReady(
    const M_CANDIDATES *const candidates, const M_CANDIDATE *const candidate)
{
    for (int32_t i = 0; i < candidate->dependencies.count; i++) {
        if (!candidates->items[candidate->dependencies.items[i]].placed) {
            return false;
        }
    }
    return true;
}

// Returns the next candidate to apply: the first in id order whose
// dependencies all apply before it, or -1.
static int32_t M_NextReady(const M_CANDIDATES *const candidates)
{
    for (int32_t i = 0; i < candidates->count; i++) {
        const M_CANDIDATE *const candidate = &candidates->items[i];
        if (!candidate->excluded && !candidate->placed
            && M_IsReady(candidates, candidate)) {
            return i;
        }
    }
    return -1;
}

static void M_ListFonts(
    PATCH_STACK *const stack, const M_CANDIDATE *const candidate,
    PATCH_INFO *const info)
{
    const JSON_VALUE *const fonts = Json_Get(candidate->patch_js, "fonts");
    if (fonts == nullptr || fonts->type != JSON_OBJECT) {
        return;
    }
    const char **const paths = Arena_AllocArr(
        &stack->arena,
        (size_t)(fonts->object.count > 0 ? fonts->object.count : 1),
        sizeof(char *));
    const ALLOCATOR *const allocator = Arena_Allocator(&stack->arena);
    for (int32_t i = 0; i < fonts->object.count; i++) {
        const JSON_MEMBER *const member = &fonts->object.members[i];
        if (member->value->type == JSON_BOOL && member->value->boolean) {
            paths[info->font_count++] =
                Path_Join(allocator, info->dir, member->key);
        }
    }
    info->fonts = paths;
}

static void M_AddPatch(PATCH_STACK *const stack, M_CANDIDATE *const candidate)
{
    PATCH *const patch = &stack->patches[stack->count++];
    *patch = (PATCH) {
        .info = {
            .id = candidate->id,
            .title = Json_GetString(
                Json_Get(candidate->patch_js, "title"), candidate->id),
            .dir = candidate->dir,
        },
    };
    candidate->id = nullptr;
    candidate->dir = nullptr;
    M_ListFonts(stack, candidate, &patch->info);
    HashMap_Init(&patch->root, &g_HeapAllocator);
    HashMap_Init(&patch->game, &g_HeapAllocator);
    M_IndexDir(&patch->root, patch->info.dir, "", stack->game.build, false, 0);
    char *const game_dir =
        Path_Join(&g_HeapAllocator, patch->info.dir, stack->game_id);
    M_IndexDir(
        &patch->game, game_dir, "", stack->game.build,
        stack->game.layout == PATCH_LAYOUT_PATHS, 0);
    Mem_Free(game_dir);

    int32_t ignored = 0;
    for (int32_t i = 0; HashMap_Next(&patch->game, &i); i++) {
        const char *const key = patch->game.keys[i];
        if (Str_EndsWith(key, ".jdiff") || stack->game.classify == nullptr) {
            continue;
        }
        const PATCH_FILE_KIND kind = stack->game.classify(key);
        if (kind == PATCH_FILE_GAMEPLAY) {
            stack->changes_gameplay = true;
        } else if (kind == PATCH_FILE_IGNORED) {
            ignored++;
        }
    }
    if (ignored > 0) {
        LOG_INFO(
            "The patch %s has %d files that are not used yet", patch->info.id,
            ignored);
    }
}

static void M_Order(PATCH_STACK *const stack, M_CANDIDATES *const candidates)
{
    stack->patches = Mem_AllocArrZ(
        (size_t)(candidates->count > 0 ? candidates->count : 1), sizeof(PATCH));
    for (;;) {
        const int32_t next = M_NextReady(candidates);
        if (next < 0) {
            break;
        }
        candidates->items[next].placed = true;
        M_AddPatch(stack, &candidates->items[next]);
    }
    for (int32_t i = 0; i < candidates->count; i++) {
        const M_CANDIDATE *const candidate = &candidates->items[i];
        if (!candidate->excluded && !candidate->placed) {
            LOG_WARNING(
                "Left out the patch %s: its dependencies form a cycle",
                candidate->id);
        }
    }
}

static void M_FreeCandidates(M_CANDIDATES *const candidates)
{
    for (int32_t i = 0; i < candidates->count; i++) {
        Mem_Free(candidates->items[i].id);
        Mem_Free(candidates->items[i].dir);
        Array_Free(&candidates->items[i].dependencies);
    }
    Array_Free(candidates);
}

char *PatchStack_GetGameKey(
    const PATCH_STACK *const stack, const char *const path)
{
    char *normalized;
    if (!Result_Ignore(
            VfsNormalize_Path(path, &g_HeapAllocator, &normalized))) {
        return nullptr;
    }
    const char *key = normalized;
    if (stack->game.layout == PATCH_LAYOUT_FLAT) {
        const char *const slash = strrchr(normalized, '/');
        if (slash != nullptr) {
            key = slash + 1;
        }
    }
    char *const copy = Str_Dup(key);
    Allocator_Free(&g_HeapAllocator, normalized, strlen(normalized) + 1);
    return copy;
}

bool PatchStack_IsOverlaid(
    const PATCH_STACK *const stack, const char *const key)
{
    return !Str_EndsWith(key, ".jdiff")
        && (stack->game.classify == nullptr
            || stack->game.classify(key) != PATCH_FILE_IGNORED);
}

JSON_VALUE *PatchStack_ReadJson(
    const char *const path, const ALLOCATOR *const allocator)
{
    READER reader;
    if (!Result_Log(Reader_OpenFile(&reader, path), LOG_LEVEL_WARNING)) {
        return nullptr;
    }
    const size_t size = Reader_Size(&reader);
    char *const text = Reader_ReadAlloc(&reader, size, &g_HeapAllocator);
    const RESULT read_result = Reader_Result(&reader);
    Reader_Close(&reader);
    JSON_VALUE *value = nullptr;
    if (Result_Log(read_result, LOG_LEVEL_WARNING)) {
        const RESULT result = Json_Parse(text, size, allocator, &value);
        if (!IS_OK(result)) {
            LOG_WARNING("%s: %s", path, result.message);
            Result_Ignore(result);
        }
    }
    if (text != nullptr) {
        Allocator_Free(&g_HeapAllocator, text, size);
    }
    return value;
}

RESULT Patch_OpenStack(
    const char *const dir, const char *const game_id,
    const PATCH_GAME *const game, const char *const *const disabled,
    const int32_t disabled_count, PATCH_STACK **const out)
{
    *out = nullptr;
    PATCH_STACK *const stack = Mem_AllocZ(sizeof(*stack));
    Arena_Init(&stack->arena, "patch stack", M_ARENA_CAPACITY);
    stack->game_id = Str_Dup(game_id);
    stack->game = *game;
    stack->game.build = Str_Dup(game->build != nullptr ? game->build : "");
    M_ToLower((char *)stack->game.build);

    M_CANDIDATES candidates;
    Array_Init(&candidates, &g_HeapAllocator);
    if (File_Exists(dir)) {
        const RESULT result = M_FindCandidates(stack, dir, &candidates);
        if (!IS_OK(result)) {
            M_FreeCandidates(&candidates);
            Patch_CloseStack(stack);
            return result;
        }
    }
    M_Exclude(&candidates, game_id, disabled, disabled_count);
    M_Order(stack, &candidates);
    M_FreeCandidates(&candidates);
    *out = stack;
    return OK;
}

void Patch_CloseStack(PATCH_STACK *const stack)
{
    if (stack == nullptr) {
        return;
    }
    for (int32_t i = 0; i < stack->count; i++) {
        PATCH *const patch = &stack->patches[i];
        Mem_Free((char *)patch->info.id);
        Mem_Free((char *)patch->info.dir);
        M_FreeFiles(&patch->root);
        M_FreeFiles(&patch->game);
    }
    Mem_Free(stack->patches);
    Mem_Free((char *)stack->game.build);
    Mem_Free(stack->game_id);
    Arena_Free(&stack->arena);
    Mem_Free(stack);
}

int32_t Patch_GetCount(const PATCH_STACK *const stack)
{
    return stack != nullptr ? stack->count : 0;
}

const PATCH_INFO *Patch_Get(const PATCH_STACK *const stack, const int32_t index)
{
    ASSERT(stack != nullptr && index >= 0 && index < stack->count);
    return &stack->patches[index].info;
}

bool Patch_ChangesGameplay(const PATCH_STACK *const stack)
{
    return stack != nullptr && stack->changes_gameplay;
}
