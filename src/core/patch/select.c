#include "core/patch/select.h"

#include "core/patch/language.h"
#include "core/patch/private.h"

#include "core/base/arena.h"
#include "core/base/array.h"
#include "core/base/assert.h"
#include "core/base/log.h"
#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/base/utils.h"
#include "core/ini/edit.h"
#include "core/io/file.h"
#include "core/io/path.h"
#include "core/io/reader.h"
#include "core/json.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#define M_ARENA_CAPACITY (64 * 1024 * 1024)

#define M_ID_WIDTH 32

typedef ARRAY(int32_t) M_INDICES;

typedef struct {
    PATCH_ENTRY entry;
    const char *name;
    M_INDICES dependencies;
    const char *missing;
    bool disabled;
    bool unsupported;
    bool in_cycle;
    bool needed;
    bool needed_by_language;
    // Unlike load, also true when the patch that needs it cannot load, so
    // that a skipped library reports why instead of "not needed".
    bool wanted;
    const char *blocker;
    bool blocker_known;
    const char *held_back;
    bool placed;
} M_PATCH;

typedef ARRAY(M_PATCH) M_PATCHES;

struct PATCH_SELECTION {
    ARENA arena;
    const char *dir;
    const char *game_id;
    M_PATCHES patches;
    M_INDICES order;
    int32_t language;
    const char *language_line;
    bool language_missing;
    bool enabled;
};

PRINTF_FORMAT(2, 3)
static const char *M_Format(ARENA *const arena, const char *const fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char *const heap = Str_FormatV(fmt, args);
    va_end(args);
    const size_t size = strlen(heap) + 1;
    char *const copy = Arena_Alloc(arena, size);
    memcpy(copy, heap, size);
    Mem_Free(heap);
    return copy;
}

static int M_ComparePatches(const void *const a, const void *const b)
{
    return strcmp(
        ((const M_PATCH *)a)->entry.id, ((const M_PATCH *)b)->entry.id);
}

static void M_AddPatch(
    PATCH_SELECTION *const selection, const char *const repo,
    const char *const name)
{
    ARENA *const arena = &selection->arena;
    const ALLOCATOR *const allocator = Arena_Allocator(arena);
    const char *const repo_dir = Path_Join(allocator, selection->dir, repo);
    const char *const patch_dir = Path_Join(allocator, repo_dir, name);
    const char *const patch_js_path =
        Path_Join(allocator, patch_dir, "patch.js");
    if (!File_Exists(patch_js_path)) {
        return;
    }
    M_PATCH patch = {
        .entry = {
            .id = M_Format(arena, "%s/%s", repo, name),
            .dir = M_Format(arena, "%s/", patch_dir),
        },
    };
    patch.name = patch.entry.id + strlen(repo) + 1;
    Array_Init(&patch.dependencies, allocator);
    const JSON_VALUE *const patch_js =
        PatchSelect_ReadJson(patch_js_path, allocator);
    if (patch_js != nullptr && patch_js->type != JSON_OBJECT) {
        LOG_WARNING("%s: not an object", patch_js_path);
    } else {
        patch.entry.patch_js = patch_js;
    }
    const char *const title =
        Json_GetString(Json_Get(patch.entry.patch_js, "title"), "");
    patch.entry.title = title[0] != '\0' ? title : patch.entry.id;
    Array_Append(&selection->patches, patch);
}

static RESULT M_FindPatches(PATCH_SELECTION *const selection)
{
    const ALLOCATOR *const allocator = Arena_Allocator(&selection->arena);
    FILE_LIST repos;
    MUST(File_ListDir(selection->dir, allocator, &repos));
    for (int32_t i = 0; i < repos.count; i++) {
        const char *const repo_dir =
            Path_Join(allocator, selection->dir, repos.items[i]);
        FILE_LIST names;
        if (Result_Ignore(File_ListDir(repo_dir, allocator, &names))) {
            for (int32_t j = 0; j < names.count; j++) {
                M_AddPatch(selection, repos.items[i], names.items[j]);
            }
        }
    }
    M_PATCHES *const patches = &selection->patches;
    if (patches->count > 1) {
        qsort(
            patches->items, (size_t)patches->count, sizeof(M_PATCH),
            M_ComparePatches);
    }
    return OK;
}

static int32_t M_FindId(const M_PATCHES *const patches, const char *const id)
{
    for (int32_t i = 0; i < patches->count; i++) {
        if (strcmp(patches->items[i].entry.id, id) == 0) {
            return i;
        }
    }
    return -1;
}

static int32_t M_ResolveDependency(
    PATCH_SELECTION *const selection, const M_PATCH *const patch,
    const char *const dependency)
{
    const M_PATCHES *const patches = &selection->patches;
    if (strchr(dependency, '/') != nullptr) {
        return M_FindId(patches, dependency);
    }
    const int repo_length = (int)(patch->name - patch->entry.id);
    const char *const same_repo = M_Format(
        &selection->arena, "%.*s%s", repo_length, patch->entry.id, dependency);
    const int32_t found = M_FindId(patches, same_repo);
    if (found >= 0) {
        return found;
    }
    for (int32_t i = 0; i < patches->count; i++) {
        if (strcmp(patches->items[i].name, dependency) == 0) {
            return i;
        }
    }
    return -1;
}

static void M_ResolveDependencies(PATCH_SELECTION *const selection)
{
    M_PATCHES *const patches = &selection->patches;
    for (int32_t i = 0; i < patches->count; i++) {
        M_PATCH *const patch = &patches->items[i];
        const JSON_VALUE *const dependencies =
            Json_Get(patch->entry.patch_js, "dependencies");
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
                M_ResolveDependency(selection, patch, dependency);
            if (found < 0) {
                if (patch->missing == nullptr) {
                    patch->missing = dependency;
                }
                continue;
            }
            Array_Append(&patch->dependencies, found);
            if (found != i) {
                M_PATCH *const target = &patches->items[found];
                target->needed = true;
                target->needed_by_language = target->needed_by_language
                    || Patch_IsLanguagePack(patch->entry.id);
            }
        }
        patch->entry.dependencies = patch->dependencies.items;
        patch->entry.dependency_count = patch->dependencies.count;
    }
}

static bool M_IsListed(
    const char *const list, const char *const id, const size_t id_length)
{
    const char *rest = list;
    while (rest != nullptr && *rest != '\0') {
        const char *const comma = strchr(rest, ',');
        const char *end = comma != nullptr ? comma : rest + strlen(rest);
        while (rest < end && (*rest == ' ' || *rest == '\t')) {
            rest++;
        }
        while (end > rest && (end[-1] == ' ' || end[-1] == '\t')) {
            end--;
        }
        if ((size_t)(end - rest) == id_length
            && strncmp(rest, id, id_length) == 0) {
            return true;
        }
        rest = comma != nullptr ? comma + 1 : nullptr;
    }
    return false;
}

static bool M_SupportsGame(
    const M_PATCH *const patch, const char *const game_id)
{
    const JSON_VALUE *const games =
        Json_Get(patch->entry.patch_js, "supported_games");
    if (game_id == nullptr || games == nullptr || games->type != JSON_ARRAY) {
        return true;
    }
    for (int32_t i = 0; i < games->array.count; i++) {
        if (strcmp(Json_GetString(Json_At(games, i), ""), game_id) == 0) {
            return true;
        }
    }
    return false;
}

static bool M_Reaches(
    const M_PATCHES *const patches, const int32_t index, const int32_t start,
    bool *const seen)
{
    const M_PATCH *const patch = &patches->items[index];
    for (int32_t i = 0; i < patch->dependencies.count; i++) {
        const int32_t next = patch->dependencies.items[i];
        if (next == start) {
            return true;
        }
        if (!seen[next]) {
            seen[next] = true;
            if (M_Reaches(patches, next, start, seen)) {
                return true;
            }
        }
    }
    return false;
}

static void M_MarkStates(
    PATCH_SELECTION *const selection, const PATCH_CHOICE *const choice)
{
    M_PATCHES *const patches = &selection->patches;
    bool *const seen = Arena_AllocArrZ(
        &selection->arena, (size_t)MAX(patches->count, 1), sizeof(bool));
    for (int32_t i = 0; i < patches->count; i++) {
        M_PATCH *const patch = &patches->items[i];
        patch->disabled = M_IsListed(
            choice->disabled, patch->entry.id, strlen(patch->entry.id));
        patch->unsupported = patch->entry.patch_js != nullptr
            && !M_SupportsGame(patch, selection->game_id);
        memset(seen, 0, (size_t)patches->count * sizeof(bool));
        patch->in_cycle = M_Reaches(patches, i, i, seen);
    }
}

static const char *M_Blocker(
    PATCH_SELECTION *const selection, const int32_t index)
{
    M_PATCH *const patch = &selection->patches.items[index];
    if (patch->blocker_known) {
        return patch->blocker;
    }
    ARENA *const arena = &selection->arena;
    const char *blocker = nullptr;
    if (patch->disabled) {
        blocker = "disabled in [patches] disabled";
    } else if (patch->entry.patch_js == nullptr) {
        blocker = "cannot read patch.js";
    } else if (patch->unsupported) {
        blocker = M_Format(arena, "does not support %s", selection->game_id);
    } else if (patch->missing != nullptr) {
        blocker =
            M_Format(arena, "needs %s, which is not installed", patch->missing);
    } else if (patch->in_cycle) {
        blocker = "its dependencies form a loop";
    }
    for (int32_t i = 0; blocker == nullptr && i < patch->dependencies.count;
         i++) {
        const int32_t dependency_index = patch->dependencies.items[i];
        const M_PATCH *const dependency =
            &selection->patches.items[dependency_index];
        if (M_Blocker(selection, dependency_index) != nullptr) {
            blocker = M_Format(
                arena, "needs %s, which %s", dependency->entry.id,
                dependency->disabled ? "is disabled" : "cannot load");
        }
    }
    patch->blocker = blocker;
    patch->blocker_known = true;
    return blocker;
}

static void M_Close(
    const M_PATCHES *const patches, const int32_t index, bool *const in,
    M_INDICES *const closure)
{
    if (in[index]) {
        return;
    }
    in[index] = true;
    Array_Append(closure, index);
    const M_PATCH *const patch = &patches->items[index];
    for (int32_t i = 0; i < patch->dependencies.count; i++) {
        M_Close(patches, patch->dependencies.items[i], in, closure);
    }
}

static int32_t M_FindLanguage(
    const M_PATCHES *const patches, const char *const tag,
    const bool *const allowed, int32_t *const disabled)
{
    const size_t prefix = strlen(PATCH_LANGUAGE_PREFIX);
    for (int32_t i = 0; i < patches->count; i++) {
        const M_PATCH *const patch = &patches->items[i];
        if ((allowed == nullptr || allowed[i])
            && Patch_IsLanguagePack(patch->entry.id)
            && Patch_LanguageIs(patch->entry.id + prefix, tag)) {
            if (!patch->disabled) {
                return i;
            }
            if (disabled != nullptr) {
                *disabled = i;
            }
        }
    }
    return -1;
}

static void M_ChooseByCode(
    PATCH_SELECTION *const selection, const char *const code)
{
    ARENA *const arena = &selection->arena;
    const M_PATCHES *const patches = &selection->patches;
    char tags[PATCH_LANGUAGE_MAX_TAGS][PATCH_LANGUAGE_TAG_SIZE];
    const int32_t tag_count = Patch_GetLanguageTags(code, tags);
    if (tag_count > 0 && Patch_IsOriginalLanguage(tags[0])) {
        selection->language_line =
            M_Format(arena, "original ([patches] language = %s)", code);
        return;
    }
    int32_t disabled = -1;
    for (int32_t i = 0; i < tag_count; i++) {
        const int32_t found =
            M_FindLanguage(patches, tags[i], nullptr, &disabled);
        if (found >= 0) {
            selection->language = found;
            selection->language_line = M_Format(
                arena, "%s ([patches] language = %s)",
                patches->items[found].entry.id, code);
            return;
        }
    }
    selection->language_missing = true;
    if (disabled >= 0) {
        selection->language_line = M_Format(
            arena, "original ([patches] language = %s, but %s is disabled)",
            code, patches->items[disabled].entry.id);
    } else {
        selection->language_line = M_Format(
            arena,
            "original ([patches] language = %s, but " PATCH_LANGUAGE_PREFIX
            "%s is not installed)",
            code, tag_count > 0 ? tags[0] : code);
    }
}

static void M_ChooseByLocale(
    PATCH_SELECTION *const selection, const PATCH_CHOICE *const choice)
{
    ARENA *const arena = &selection->arena;
    const M_PATCHES *const patches = &selection->patches;
    bool *const top =
        Arena_AllocArrZ(arena, (size_t)MAX(patches->count, 1), sizeof(bool));
    int32_t top_count = 0;
    int32_t only = -1;
    bool any_pack = false;
    for (int32_t i = 0; i < patches->count; i++) {
        const M_PATCH *const patch = &patches->items[i];
        if (!Patch_IsLanguagePack(patch->entry.id)) {
            continue;
        }
        any_pack = true;
        if (!patch->needed_by_language && !patch->disabled) {
            top[i] = true;
            top_count++;
            only = i;
        }
    }
    if (top_count == 0) {
        selection->language_line = any_pack
            ? "original (every language is disabled)"
            : "original (no language installed)";
        return;
    }
    if (top_count == 1) {
        selection->language = only;
        selection->language_line = M_Format(
            arena, "%s (the only installed language)",
            patches->items[only].entry.id);
        return;
    }
    for (int32_t i = 0; i < choice->locale_count; i++) {
        char code[PATCH_LANGUAGE_TAG_SIZE];
        Patch_GetLocaleCode(choice->locales[i], code);
        char tags[PATCH_LANGUAGE_MAX_TAGS][PATCH_LANGUAGE_TAG_SIZE];
        const int32_t tag_count = Patch_GetLanguageTags(code, tags);
        if (tag_count > 0 && Patch_IsOriginalLanguage(tags[0])) {
            selection->language_line =
                M_Format(arena, "original (system language %s)", code);
            return;
        }
        for (int32_t j = 0; j < tag_count; j++) {
            const int32_t found =
                M_FindLanguage(patches, tags[j], top, nullptr);
            if (found >= 0) {
                selection->language = found;
                selection->language_line = M_Format(
                    arena, "%s (system language %s)",
                    patches->items[found].entry.id, code);
                return;
            }
        }
    }
    selection->language_line =
        "original (no installed language matches the system languages)";
}

static void M_ChooseLanguage(
    PATCH_SELECTION *const selection, const PATCH_CHOICE *const choice)
{
    selection->language = -1;
    if (!choice->enabled) {
        selection->language_line = "original ([patches] enabled = false)";
    } else if (choice->language != nullptr && choice->language[0] != '\0') {
        M_ChooseByCode(selection, choice->language);
    } else {
        M_ChooseByLocale(selection, choice);
    }
}

static void M_SetRoles(PATCH_SELECTION *const selection)
{
    M_PATCHES *const patches = &selection->patches;
    for (int32_t i = 0; i < patches->count; i++) {
        M_PATCH *const patch = &patches->items[i];
        if (i == selection->language) {
            patch->entry.role = PATCH_ROLE_LANGUAGE;
        } else if (patch->needed) {
            patch->entry.role = PATCH_ROLE_LIBRARY;
        } else if (Patch_IsLanguagePack(patch->entry.id)) {
            patch->entry.role = PATCH_ROLE_LANGUAGE;
        } else {
            patch->entry.role = PATCH_ROLE_MOD;
        }
    }
}

static int32_t M_OutsideLanguage(
    const M_PATCHES *const patches, const M_INDICES *const closure,
    const bool *const allowed)
{
    for (int32_t i = 0; i < closure->count; i++) {
        const int32_t index = closure->items[i];
        if (!allowed[index]
            && Patch_IsLanguagePack(patches->items[index].entry.id)) {
            return index;
        }
    }
    return -1;
}

static void M_Load(M_PATCHES *const patches, const M_INDICES *const closure)
{
    for (int32_t i = 0; i < closure->count; i++) {
        patches->items[closure->items[i]].entry.load = true;
    }
}

static void M_Decide(PATCH_SELECTION *const selection)
{
    if (!selection->enabled) {
        return;
    }
    ARENA *const arena = &selection->arena;
    M_PATCHES *const patches = &selection->patches;
    const size_t count = (size_t)MAX(patches->count, 1);
    bool *const allowed = Arena_AllocArrZ(arena, count, sizeof(bool));
    bool *const in = Arena_AllocArrZ(arena, count, sizeof(bool));
    M_INDICES closure;
    Array_Init(&closure, Arena_Allocator(arena));

    const int32_t language = selection->language;
    if (language >= 0) {
        M_Close(patches, language, allowed, &closure);
        for (int32_t i = 0; i < closure.count; i++) {
            patches->items[closure.items[i]].wanted = true;
        }
        if (M_Blocker(selection, language) == nullptr) {
            M_Load(patches, &closure);
        }
    }
    for (int32_t i = 0; i < patches->count; i++) {
        M_PATCH *const mod = &patches->items[i];
        if (mod->entry.role != PATCH_ROLE_MOD || mod->disabled) {
            continue;
        }
        memset(in, 0, count * sizeof(bool));
        Array_Clear(&closure);
        M_Close(patches, i, in, &closure);
        const int32_t outside = M_OutsideLanguage(patches, &closure, allowed);
        if (outside >= 0) {
            mod->held_back = M_Format(
                arena, "needs %s, outside the chosen language",
                patches->items[outside].entry.id);
            continue;
        }
        for (int32_t j = 0; j < closure.count; j++) {
            patches->items[closure.items[j]].wanted = true;
        }
        if (M_Blocker(selection, i) == nullptr) {
            M_Load(patches, &closure);
        }
    }
}

static bool M_IsReady(
    const M_PATCHES *const patches, const M_PATCH *const patch)
{
    for (int32_t i = 0; i < patch->dependencies.count; i++) {
        if (!patches->items[patch->dependencies.items[i]].placed) {
            return false;
        }
    }
    return true;
}

static void M_Place(PATCH_SELECTION *const selection, const bool *const in)
{
    M_PATCHES *const patches = &selection->patches;
    for (;;) {
        int32_t next = -1;
        for (int32_t i = 0; i < patches->count && next < 0; i++) {
            const M_PATCH *const patch = &patches->items[i];
            if (in[i] && patch->entry.load && !patch->placed
                && M_IsReady(patches, patch)) {
                next = i;
            }
        }
        if (next < 0) {
            return;
        }
        patches->items[next].placed = true;
        Array_Append(&selection->order, next);
    }
}

static void M_Order(PATCH_SELECTION *const selection)
{
    M_PATCHES *const patches = &selection->patches;
    const size_t count = (size_t)MAX(patches->count, 1);
    bool *const in = Arena_AllocArrZ(&selection->arena, count, sizeof(bool));
    if (selection->language >= 0
        && patches->items[selection->language].entry.load) {
        M_INDICES closure;
        Array_Init(&closure, Arena_Allocator(&selection->arena));
        M_Close(patches, selection->language, in, &closure);
        M_Place(selection, in);
    }
    memset(in, 1, count * sizeof(bool));
    M_Place(selection, in);
}

static const char *M_NeededBy(
    PATCH_SELECTION *const selection, const int32_t index)
{
    const char *text = "";
    for (int32_t i = 0; i < selection->order.count; i++) {
        const int32_t user = selection->order.items[i];
        const M_PATCH *const patch = &selection->patches.items[user];
        bool needs = false;
        for (int32_t j = 0; j < patch->dependencies.count; j++) {
            needs = needs
                || (user != index && patch->dependencies.items[j] == index);
        }
        if (needs) {
            text = M_Format(
                &selection->arena, "%s%s%s", text, text[0] != '\0' ? ", " : "",
                patch->entry.id);
        }
    }
    return text;
}

static const char *M_SkipReason(
    PATCH_SELECTION *const selection, const int32_t index)
{
    M_PATCH *const patch = &selection->patches.items[index];
    const PATCH_ROLE role = patch->entry.role;
    if (role == PATCH_ROLE_MOD || index == selection->language
        || (role == PATCH_ROLE_LIBRARY && patch->wanted)) {
        const char *const blocker = patch->held_back != nullptr
            ? patch->held_back
            : M_Blocker(selection, index);
        if (blocker != nullptr) {
            return blocker;
        }
    }
    return role == PATCH_ROLE_LANGUAGE ? "not chosen" : "not needed";
}

static void M_SetReasons(PATCH_SELECTION *const selection)
{
    ARENA *const arena = &selection->arena;
    M_PATCHES *const patches = &selection->patches;
    for (int32_t i = 0; i < patches->count; i++) {
        M_PATCH *const patch = &patches->items[i];
        const char *const role = Patch_GetRoleName(patch->entry.role);
        if (patch->disabled) {
            patch->entry.reason = "disabled in [patches] disabled";
        } else if (!selection->enabled) {
            patch->entry.reason =
                M_Format(arena, "%s, [patches] enabled = false", role);
        } else if (!patch->entry.load) {
            patch->entry.reason =
                M_Format(arena, "%s, %s", role, M_SkipReason(selection, i));
        } else if (patch->entry.role == PATCH_ROLE_LIBRARY) {
            patch->entry.reason = M_Format(
                arena, "library, needed by %s", M_NeededBy(selection, i));
        } else {
            patch->entry.reason = role;
        }
    }
}

PRINTF_FORMAT(2, 3)
static void M_Append(char **const text, const char *const fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char *const line = Str_FormatV(fmt, args);
    va_end(args);
    char *const longer = Str_Format("%s%s", *text, line);
    Mem_Free(line);
    Mem_Free(*text);
    *text = longer;
}

static void M_AppendEntry(char **const text, const M_PATCH *const patch)
{
    M_Append(
        text, "  %s  %-*s %s\n", patch->entry.load ? "load" : "skip",
        M_ID_WIDTH, patch->entry.id, patch->entry.reason);
}

static void M_AppendTree(
    char **const text, const M_PATCHES *const patches, const int32_t index,
    bool *const listed)
{
    const M_PATCH *const patch = &patches->items[index];
    if (listed[index] || !patch->entry.load) {
        return;
    }
    listed[index] = true;
    M_AppendEntry(text, patch);
    for (int32_t i = 0; i < patch->dependencies.count; i++) {
        M_AppendTree(text, patches, patch->dependencies.items[i], listed);
    }
}

static int32_t M_SkipGroup(const M_PATCH *const patch)
{
    if (patch->disabled) {
        return 3;
    }
    switch (patch->entry.role) {
    case PATCH_ROLE_LANGUAGE:
        return 0;
    case PATCH_ROLE_LIBRARY:
        return 1;
    case PATCH_ROLE_MOD:
        return 2;
    }
    return 2;
}

static bool M_IsWord(const char *const text, const char *const *const words)
{
    for (int32_t i = 0; words[i] != nullptr; i++) {
        if (Str_EqualIgnoreCase(text, words[i])) {
            return true;
        }
    }
    return false;
}

JSON_VALUE *PatchSelect_ReadJson(
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

void Patch_ReadChoice(const INI *const ini, PATCH_CHOICE *const choice)
{
    static const char *const no[] = { "false", "no", "off", "0", nullptr };
    static const char *const yes[] = { "true", "yes", "on", "1", nullptr };
    const char *const enabled = Ini_Get(ini, "patches", "enabled");
    choice->enabled = enabled == nullptr || !M_IsWord(enabled, no);
    if (enabled != nullptr && choice->enabled && !M_IsWord(enabled, yes)) {
        LOG_WARNING(
            "patches.enabled: invalid value \"%s\", keeping the default",
            enabled);
    }
    choice->disabled = Ini_Get(ini, "patches", "disabled");
    choice->language = Ini_Get(ini, "patches", "language");
}

RESULT Patch_Select(
    const char *const dir, const char *const game_id,
    const PATCH_CHOICE *const choice, PATCH_SELECTION **const out)
{
    *out = nullptr;
    static const PATCH_CHOICE fallback = { .enabled = true };
    const PATCH_CHOICE *const used = choice != nullptr ? choice : &fallback;
    PATCH_SELECTION *const selection = Mem_AllocZ(sizeof(*selection));
    Arena_Init(&selection->arena, "patch selection", M_ARENA_CAPACITY);
    ARENA *const arena = &selection->arena;
    selection->dir = M_Format(arena, "%s", dir);
    selection->game_id =
        game_id != nullptr ? M_Format(arena, "%s", game_id) : nullptr;
    selection->enabled = used->enabled;
    Array_Init(&selection->patches, Arena_Allocator(arena));
    Array_Init(&selection->order, Arena_Allocator(arena));
    if (File_Exists(dir)) {
        const RESULT result = M_FindPatches(selection);
        if (!IS_OK(result)) {
            Patch_FreeSelection(selection);
            return result;
        }
    }
    M_ResolveDependencies(selection);
    M_MarkStates(selection, used);
    M_ChooseLanguage(selection, used);
    M_SetRoles(selection);
    M_Decide(selection);
    M_Order(selection);
    M_SetReasons(selection);
    *out = selection;
    return OK;
}

void Patch_FreeSelection(PATCH_SELECTION *const selection)
{
    if (selection == nullptr) {
        return;
    }
    Arena_Free(&selection->arena);
    Mem_Free(selection);
}

const char *Patch_GetRoleName(const PATCH_ROLE role)
{
    switch (role) {
    case PATCH_ROLE_LANGUAGE:
        return "language";
    case PATCH_ROLE_LIBRARY:
        return "library";
    case PATCH_ROLE_MOD:
        return "mod";
    }
    return "mod";
}

int32_t Patch_GetEntryCount(const PATCH_SELECTION *const selection)
{
    return selection->patches.count;
}

const PATCH_ENTRY *Patch_GetEntry(
    const PATCH_SELECTION *const selection, const int32_t index)
{
    ASSERT(index >= 0 && index < selection->patches.count);
    return &selection->patches.items[index].entry;
}

int32_t Patch_GetLoadCount(const PATCH_SELECTION *const selection)
{
    return selection->order.count;
}

const PATCH_ENTRY *Patch_GetLoad(
    const PATCH_SELECTION *const selection, const int32_t index)
{
    ASSERT(index >= 0 && index < selection->order.count);
    return &selection->patches.items[selection->order.items[index]].entry;
}

bool Patch_IsLanguageMissing(const PATCH_SELECTION *const selection)
{
    return selection->language_missing;
}

char *Patch_DescribeSelection(const PATCH_SELECTION *const selection)
{
    const M_PATCHES *const patches = &selection->patches;
    size_t dir_length = strlen(selection->dir);
    if (dir_length > 1
        && (selection->dir[dir_length - 1] == '/'
            || selection->dir[dir_length - 1] == '\\')) {
        dir_length--;
    }
    char *text = Str_Format(
        "Patches in %.*s:\n  language: %s\n", (int)dir_length, selection->dir,
        selection->language_line);
    bool *const listed =
        Mem_AllocArrZ((size_t)MAX(patches->count, 1), sizeof(bool));
    if (selection->language >= 0) {
        M_AppendTree(&text, patches, selection->language, listed);
    }
    for (int32_t i = 0; i < patches->count; i++) {
        if (patches->items[i].entry.role == PATCH_ROLE_MOD) {
            M_AppendTree(&text, patches, i, listed);
        }
    }
    Mem_Free(listed);
    for (int32_t group = 0; group < 4; group++) {
        for (int32_t i = 0; i < patches->count; i++) {
            const M_PATCH *const patch = &patches->items[i];
            if (!patch->entry.load && M_SkipGroup(patch) == group) {
                M_AppendEntry(&text, patch);
            }
        }
    }
    M_Append(&text, "Load order: ");
    for (int32_t i = 0; i < selection->order.count; i++) {
        M_Append(
            &text, "%s%s", i > 0 ? ", " : "", Patch_GetLoad(selection, i)->id);
    }
    M_Append(&text, "%s\n", selection->order.count == 0 ? "none" : "");
    return text;
}
