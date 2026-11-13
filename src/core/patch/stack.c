#include "core/patch/private.h"

#include "core/base/assert.h"
#include "core/base/log.h"
#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/base/utils.h"
#include "core/io/file.h"
#include "core/io/path.h"
#include "core/json.h"
#include "core/vfs/normalize.h"

#include <string.h>

#define M_ARENA_CAPACITY (64 * 1024 * 1024)
// Caps how deep a game directory may nest, so that a symbolic link cycle
// cannot walk forever.
#define M_MAX_DEPTH 32

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
        FILE_TYPE type;
        if (!Result_Ignore(File_GetType(disk_path, &type))) {
            Mem_Free(disk_path);
            continue;
        }
        if (type == FILE_TYPE_DIR) {
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

static void M_ListFonts(
    PATCH_STACK *const stack, const PATCH_ENTRY *const entry,
    PATCH_INFO *const info)
{
    const JSON_VALUE *const fonts = Json_Get(entry->patch_js, "fonts");
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

static void M_AddPatch(PATCH_STACK *const stack, const PATCH_ENTRY *const entry)
{
    PATCH *const patch = &stack->patches[stack->count++];
    *patch = (PATCH) {
        .info = {
            .id = entry->id,
            .title = Json_GetString(Json_Get(entry->patch_js, "title"), entry->id),
            .dir = entry->dir,
        },
    };
    M_ListFonts(stack, entry, &patch->info);
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

static void M_Log(const PATCH_SELECTION *const selection)
{
    char *const text = Patch_DescribeSelection(selection);
    for (char *line = text; *line != '\0';) {
        char *const end = strchr(line, '\n');
        *end = '\0';
        LOG_INFO("%s", line);
        line = end + 1;
    }
    Mem_Free(text);
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

RESULT Patch_OpenStack(
    const char *const dir, const char *const game_id,
    const PATCH_GAME *const game, const PATCH_CHOICE *const choice,
    PATCH_STACK **const out)
{
    *out = nullptr;
    PATCH_SELECTION *selection;
    MUST(Patch_Select(dir, game_id, choice, &selection));
    M_Log(selection);
    if (Patch_IsLanguageMissing(selection)) {
        LOG_WARNING(
            "[patches] language = %s names no installed language pack, so "
            "the game uses the original language",
            choice->language);
    }
    PATCH_STACK *const stack = Mem_AllocZ(sizeof(*stack));
    Arena_Init(&stack->arena, "patch stack", M_ARENA_CAPACITY);
    stack->selection = selection;
    stack->game_id = Str_Dup(game_id);
    stack->game = *game;
    stack->game.build = Str_Dup(game->build != nullptr ? game->build : "");
    M_ToLower((char *)stack->game.build);
    const int32_t count = Patch_GetLoadCount(selection);
    stack->patches = Mem_AllocArrZ((size_t)MAX(count, 1), sizeof(PATCH));
    for (int32_t i = 0; i < count; i++) {
        M_AddPatch(stack, Patch_GetLoad(selection, i));
    }
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
        M_FreeFiles(&patch->root);
        M_FreeFiles(&patch->game);
    }
    Mem_Free(stack->patches);
    Mem_Free((char *)stack->game.build);
    Mem_Free(stack->game_id);
    Arena_Free(&stack->arena);
    Patch_FreeSelection(stack->selection);
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
