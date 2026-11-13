#include "core/patch/manager.h"

#include "core/patch/language.h"
#include "core/patch/repo.h"
#include "core/patch/update.h"

#include "core/base/arena.h"
#include "core/base/assert.h"
#include "core/base/log.h"
#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/base/utils.h"
#include "core/io/path.h"
#include "core/json/get.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

// Sets the size of the arena that holds the repository network. repo.c
// limits the size of each repo.js and patch.js, so that a network of
// hundreds of repositories fits.
#define M_ARENA_SIZE ((size_t)256 * 1024 * 1024)

// Sets how many repositories the message of an ambiguous name names.
#define M_MAX_MATCHES 8

struct PATCH_MANAGER {
    ARENA arena;
    const char *dir;
    const char *repo_url;
    const char **games;
    int32_t game_count;
    PATCH_FETCH_FN fetch;
    PATCH_NETWORK *network;
    // Holds the patches that this manager brought up to date.
    PATCH_REMOTE_LIST done;
    PATCH_SELECTION *selection;
    // Holds the patches of the first repository once listed.
    PATCH_LISTED *listed;
    int32_t listed_count;
};

PRINTF_FORMAT(2, 3)
static char *M_Format(ARENA *const arena, const char *const fmt, ...)
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

static void M_Select(PATCH_MANAGER *const manager)
{
    Patch_FreeSelection(manager->selection);
    manager->selection = nullptr;
    if (!Result_Log(
            Patch_Select(manager->dir, nullptr, nullptr, &manager->selection),
            LOG_LEVEL_WARNING)) {
        manager->selection = nullptr;
    }
}

static RESULT M_Connect(PATCH_MANAGER *const manager)
{
    if (manager->network == nullptr) {
        MUST(PatchRepo_Open(
            manager->repo_url, manager->fetch, &manager->arena,
            &manager->network));
    }
    return OK;
}

static bool M_Contains(
    const PATCH_REMOTE_LIST *const list, const char *const id)
{
    for (int32_t i = 0; i < list->count; i++) {
        if (strcmp(list->items[i]->id, id) == 0) {
            return true;
        }
    }
    return false;
}

static const PATCH_ENTRY *M_Entry(
    PATCH_MANAGER *const manager, const PATCH_REMOTE *const patch)
{
    ARENA *const arena = &manager->arena;
    PATCH_ENTRY *const entry = Arena_AllocZ(arena, sizeof(*entry));
    entry->id = patch->id;
    entry->title = patch->title;
    entry->dir = M_Format(
        arena, "%s/",
        Path_Join(Arena_Allocator(arena), manager->dir, patch->id));
    entry->patch_js = patch->json;
    return entry;
}

// Adds the patch name to list, after each patch that it needs.
static RESULT M_Resolve(
    PATCH_MANAGER *const manager, const char *const name,
    PATCH_REMOTE_LIST *const list)
{
    MUST(M_Connect(manager));
    const char *id = name;
    if (strchr(name, '/') == nullptr) {
        const PATCH_REPO *repos[M_MAX_MATCHES];
        const int32_t count = PatchRepo_FindAllPatch(
            manager->network, name, repos, M_MAX_MATCHES);
        if (count > 1) {
            const char *text = M_Format(
                &manager->arena, "%d repositories have a patch %s:", count,
                name);
            for (int32_t i = 0; i < MIN(count, M_MAX_MATCHES); i++) {
                text = M_Format(
                    &manager->arena, "%s%s %s/%s", text, i > 0 ? "," : "",
                    repos[i]->id, name);
            }
            return FAIL("%s. Name one as <repo>/<patch>", text);
        }
        if (count == 1) {
            id = M_Format(&manager->arena, "%s/%s", repos[0]->id, name);
        }
    }
    MUST(
        PatchRepo_AddPatch(manager->network, name, list),
        "could not install %s", id);
    return OK;
}

static void M_Fail(
    const PATCH_EVENTS *const events, const PATCH_ENTRY *const entry,
    const char *const reason)
{
    if (events != nullptr && events->patch_failed != nullptr) {
        events->patch_failed(events->user, entry, reason);
    }
}

static RESULT M_Update(
    PATCH_MANAGER *const manager, const PATCH_REMOTE *const patch,
    const PATCH_EVENTS *const events)
{
    const PATCH_ENTRY *const entry = M_Entry(manager, patch);
    PATCH_COUNTS counts;
    const RESULT result = PatchUpdate_Patch(
        manager->dir, patch, manager->games, manager->game_count,
        manager->fetch, events, entry, &counts);
    if (!IS_OK(result)) {
        M_Fail(events, entry, result.message);
        return Result_Prefix(result, "could not download %s", patch->id);
    }
    if (events != nullptr && events->patch_done != nullptr) {
        events->patch_done(events->user, entry, &counts);
    }
    Array_Append(&manager->done, patch);
    return OK;
}

// Fails when the language pack patch lists no file for the games.
static RESULT M_CheckFiles(
    PATCH_MANAGER *const manager, const PATCH_REMOTE *const patch,
    const PATCH_EVENTS *const events)
{
    if (!Patch_IsLanguagePack(patch->id)) {
        return OK;
    }
    bool has_files;
    const RESULT checked = PatchUpdate_HasGameFiles(
        patch, manager->games, manager->game_count, manager->fetch, &has_files);
    if (!IS_OK(checked)) {
        M_Fail(events, M_Entry(manager, patch), checked.message);
        return Result_Prefix(checked, "could not download %s", patch->id);
    }
    if (has_files) {
        return OK;
    }
    ARENA *const arena = &manager->arena;
    const char *programs = "";
    for (int32_t i = 0; i < manager->game_count; i++) {
        programs = M_Format(
            arena, "%s%so%s", programs, i > 0 ? ", " : "", manager->games[i]);
    }
    if (strcmp(patch->title, patch->id) == 0) {
        return FAIL("%s has no files for %s", patch->id, programs);
    }
    return FAIL(
        "%s (%s) has no files for %s", patch->title, patch->id, programs);
}

// Adds the installed patches that update to list, and reports patch_kept
// for each that the network cannot give. Sets *any to whether one updates.
static RESULT M_AddInstalled(
    PATCH_MANAGER *const manager, PATCH_REMOTE_LIST *const list,
    const PATCH_EVENTS *const events, bool *const any)
{
    *any = false;
    PATCH_INSTALLED_LIST installed;
    MUST(PatchUpdate_ListInstalled(manager->dir, &manager->arena, &installed));
    for (int32_t i = 0; i < installed.count; i++) {
        *any = *any
            || (installed.items[i].update
                && !M_Contains(&manager->done, installed.items[i].id));
    }
    if (!*any) {
        return OK;
    }
    MUST(M_Connect(manager));
    for (int32_t i = 0; i < installed.count; i++) {
        const PATCH_INSTALLED *const patch = &installed.items[i];
        if (!patch->update || M_Contains(list, patch->id)
            || M_Contains(&manager->done, patch->id)) {
            continue;
        }
        const RESULT added =
            PatchRepo_AddPatch(manager->network, patch->id, list);
        if (!IS_OK(added) && events != nullptr
            && events->patch_kept != nullptr) {
            events->patch_kept(events->user, patch->id, added.message);
        }
        Result_Ignore(added);
    }
    return OK;
}

// Appends to order the patches of list from start that this manager did
// not bring up to date and order lacks, and reports patch_queued for each.
// With reverse, the patches come from the last, so that a patch comes
// before what it needs.
static void M_Queue(
    PATCH_MANAGER *const manager, const PATCH_REMOTE_LIST *const list,
    const int32_t start, const bool reverse, PATCH_REMOTE_LIST *const order,
    const PATCH_EVENTS *const events)
{
    for (int32_t i = start; i < list->count; i++) {
        const PATCH_REMOTE *const patch =
            list->items[reverse ? list->count - 1 - (i - start) : i];
        if (M_Contains(&manager->done, patch->id)
            || M_Contains(order, patch->id)) {
            continue;
        }
        Array_Append(order, patch);
        if (events != nullptr && events->patch_queued != nullptr) {
            events->patch_queued(events->user, M_Entry(manager, patch));
        }
    }
}

static RESULT M_UpdateAll(
    PATCH_MANAGER *const manager, const PATCH_REMOTE_LIST *const order,
    const PATCH_EVENTS *const events)
{
    for (int32_t i = 0; i < order->count; i++) {
        MUST(M_Update(manager, order->items[i], events));
    }
    return OK;
}

static int M_CompareListed(const void *const a, const void *const b)
{
    return strcmp(((const PATCH_LISTED *)a)->id, ((const PATCH_LISTED *)b)->id);
}

static bool M_NeedsAny(const PATCH_ENTRY *const entry, const bool *const set)
{
    for (int32_t i = 0; i < entry->dependency_count; i++) {
        if (set[entry->dependencies[i]]) {
            return true;
        }
    }
    return false;
}

// Marks each patch that only the marked patches need. Keeps language
// packs, so that removing a mod never removes the language that a game
// uses.
static void M_MarkRemovals(
    const PATCH_MANAGER *const manager, bool *const removed)
{
    const int32_t count = PatchManager_GetCount(manager);
    bool changed = true;
    while (changed) {
        changed = false;
        for (int32_t i = 0; i < count; i++) {
            const PATCH_ENTRY *const entry = PatchManager_Get(manager, i);
            if (removed[i] || Patch_IsLanguagePack(entry->id)) {
                continue;
            }
            bool needed_by_removed = false;
            bool needed_by_kept = false;
            for (int32_t j = 0; j < count; j++) {
                const PATCH_ENTRY *const user = PatchManager_Get(manager, j);
                bool needs = false;
                for (int32_t k = 0; k < user->dependency_count; k++) {
                    needs = needs || (j != i && user->dependencies[k] == i);
                }
                needed_by_removed = needed_by_removed || (needs && removed[j]);
                needed_by_kept = needed_by_kept || (needs && !removed[j]);
            }
            if (needed_by_removed && !needed_by_kept) {
                removed[i] = true;
                changed = true;
            }
        }
    }
}

static RESULT M_Remove(
    PATCH_MANAGER *const manager, const char *const id,
    const PATCH_EVENTS *const events)
{
    const int32_t count = PatchManager_GetCount(manager);
    int32_t index = -1;
    for (int32_t i = 0; i < count; i++) {
        if (strcmp(PatchManager_Get(manager, i)->id, id) == 0) {
            index = i;
        }
    }
    FAIL_IF(index < 0, "there is no installed patch %s", id);
    bool *const removed =
        Arena_AllocArrZ(&manager->arena, (size_t)count, sizeof(bool));
    removed[index] = true;
    for (int32_t i = 0; i < count; i++) {
        const PATCH_ENTRY *const entry = PatchManager_Get(manager, i);
        FAIL_IF(
            i != index && M_NeedsAny(entry, removed), "%s needs %s", entry->id,
            id);
    }
    M_MarkRemovals(manager, removed);
    for (int32_t i = 0; i < count; i++) {
        if (!removed[i]) {
            continue;
        }
        const char *const removed_id = PatchManager_Get(manager, i)->id;
        MUST(
            PatchUpdate_RemovePatch(manager->dir, removed_id), "%s",
            removed_id);
        if (events != nullptr && events->patch_removed != nullptr) {
            events->patch_removed(events->user, removed_id);
        }
    }
    return OK;
}

PATCH_MANAGER *PatchManager_Open(
    const char *const dir, const char *const repo_url,
    const char *const *const games, const int32_t game_count,
    const PATCH_FETCH_FN fetch)
{
    PATCH_MANAGER *const manager = Mem_AllocZ(sizeof(*manager));
    ARENA *const arena = &manager->arena;
    Arena_Init(arena, "patches", M_ARENA_SIZE);
    manager->dir = M_Format(arena, "%s", dir);
    manager->repo_url = M_Format(arena, "%s", repo_url);
    manager->games =
        Arena_AllocArr(arena, (size_t)MAX(game_count, 1), sizeof(char *));
    for (int32_t i = 0; i < game_count; i++) {
        manager->games[i] = M_Format(arena, "%s", games[i]);
    }
    manager->game_count = game_count;
    manager->fetch = fetch;
    Array_Init(&manager->done, Arena_Allocator(arena));
    M_Select(manager);
    return manager;
}

int32_t PatchManager_GetCount(const PATCH_MANAGER *const manager)
{
    return manager->selection != nullptr
        ? Patch_GetEntryCount(manager->selection)
        : 0;
}

const PATCH_ENTRY *PatchManager_Get(
    const PATCH_MANAGER *const manager, const int32_t i)
{
    ASSERT(manager->selection != nullptr);
    return Patch_GetEntry(manager->selection, i);
}

RESULT PatchManager_ListPatches(
    PATCH_MANAGER *const manager, const PATCH_LISTED **const out,
    int32_t *const count)
{
    *out = nullptr;
    *count = 0;
    MUST(M_Connect(manager));
    if (manager->listed == nullptr) {
        const PATCH_REPO *const first = PatchRepo_GetFirst(manager->network);
        const JSON_VALUE *const patches = first->patches;
        const int32_t total = patches != nullptr ? patches->object.count : 0;
        ARENA *const arena = &manager->arena;
        manager->listed = Arena_AllocArrZ(
            arena, (size_t)MAX(total, 1), sizeof(*manager->listed));
        for (int32_t i = 0; i < total; i++) {
            const JSON_MEMBER *const member = &patches->object.members[i];
            manager->listed[i] = (PATCH_LISTED) {
                .id = M_Format(arena, "%s/%s", first->id, member->key),
                .title = Json_GetString(member->value, ""),
            };
        }
        qsort(
            manager->listed, (size_t)total, sizeof(*manager->listed),
            M_CompareListed);
        manager->listed_count = total;
    }
    *out = manager->listed;
    *count = manager->listed_count;
    return OK;
}

RESULT PatchManager_Install(
    PATCH_MANAGER *const manager, const char *const name,
    const PATCH_EVENTS *const events)
{
    const ALLOCATOR *const allocator = Arena_Allocator(&manager->arena);
    PATCH_REMOTE_LIST list;
    Array_Init(&list, allocator);
    MUST(M_Resolve(manager, name, &list));
    const int32_t target_count = list.count;
    MUST(M_CheckFiles(manager, list.items[target_count - 1], events));
    PATCH_REMOTE_LIST order;
    Array_Init(&order, allocator);
    M_Queue(manager, &list, 0, true, &order, events);
    bool any;
    RESULT result = M_AddInstalled(manager, &list, events, &any);
    if (IS_OK(result)) {
        M_Queue(manager, &list, target_count, false, &order, events);
        result = M_UpdateAll(manager, &order, events);
    }
    M_Select(manager);
    return result;
}

RESULT PatchManager_Update(
    PATCH_MANAGER *const manager, const PATCH_EVENTS *const events)
{
    const ALLOCATOR *const allocator = Arena_Allocator(&manager->arena);
    PATCH_REMOTE_LIST list;
    Array_Init(&list, allocator);
    bool any;
    MUST(M_AddInstalled(manager, &list, events, &any));
    if (!any) {
        return OK;
    }
    PATCH_REMOTE_LIST order;
    Array_Init(&order, allocator);
    M_Queue(manager, &list, 0, false, &order, events);
    const RESULT result = M_UpdateAll(manager, &order, events);
    M_Select(manager);
    return result;
}

RESULT PatchManager_Remove(
    PATCH_MANAGER *const manager, const char *const id,
    const PATCH_EVENTS *const events)
{
    M_Select(manager);
    const RESULT result = M_Remove(manager, id, events);
    M_Select(manager);
    return result;
}

void PatchManager_Close(PATCH_MANAGER *const manager)
{
    if (manager == nullptr) {
        return;
    }
    Patch_FreeSelection(manager->selection);
    Arena_Free(&manager->arena);
    Mem_Free(manager);
}
