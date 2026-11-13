// SPDX-License-Identifier: GPL-3.0-only

#include "othpatch/repo.h"

#include "othpatch/fetch.h"

#include "core/base/string.h"
#include "core/json/get.h"
#include "core/json/parse.h"

#include <stdarg.h>
#include <string.h>

// Sets the largest repo.js and patch.js that the walk accepts.
#define M_MAX_JSON_SIZE ((size_t)1024 * 1024)

typedef ARRAY(const char *) M_URL_LIST;
typedef ARRAY(REPO *) M_REPO_LIST;
typedef ARRAY(REPO_PATCH *) M_PATCH_LIST;

struct REPO_NETWORK {
    ARENA *arena;
    M_REPO_LIST repos;
    // Holds every URL that the walk has queued, in order; the ones from
    // next on are still to be fetched.
    M_URL_LIST urls;
    int32_t next;
    M_PATCH_LIST patches;
};

static char *M_Dup(ARENA *const arena, const char *const text)
{
    const size_t size = strlen(text) + 1;
    char *const copy = Arena_Alloc(arena, size);
    memcpy(copy, text, size);
    return copy;
}

PRINTF_FORMAT(2, 3)
static char *M_Format(ARENA *const arena, const char *const fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char *const heap = Str_FormatV(fmt, args);
    va_end(args);
    char *const copy = M_Dup(arena, heap);
    Mem_Free(heap);
    return copy;
}

// Returns url with a '/' at its end.
static const char *M_FolderUrl(ARENA *const arena, const char *const url)
{
    return Str_EndsWith(url, "/") ? M_Dup(arena, url)
                                  : M_Format(arena, "%s/", url);
}

static bool M_IsSafeName(const char *const name)
{
    if (name == nullptr || name[0] == '\0' || strcmp(name, ".") == 0
        || strcmp(name, "..") == 0) {
        return false;
    }
    for (const char *c = name; *c != '\0'; c++) {
        if ((unsigned char)*c < 0x20 || strchr("/\\:*?\"<>|", *c) != nullptr) {
            return false;
        }
    }
    return true;
}

static void M_Queue(REPO_NETWORK *const network, const char *const url)
{
    const char *const folder = M_FolderUrl(network->arena, url);
    for (int32_t i = 0; i < network->urls.count; i++) {
        if (strcmp(network->urls.items[i], folder) == 0) {
            return;
        }
    }
    Array_Append(&network->urls, folder);
}

static const REPO *M_KnownRepo(
    const REPO_NETWORK *const network, const char *const id)
{
    for (int32_t i = 0; i < network->repos.count; i++) {
        if (strcmp(network->repos.items[i]->id, id) == 0) {
            return network->repos.items[i];
        }
    }
    return nullptr;
}

// Parses a fetched repo.js, adds the repository and queues its servers and
// neighbors.
static RESULT M_AddRepo(
    REPO_NETWORK *const network, const char *const url,
    const FETCH *const fetch)
{
    ARENA *const arena = network->arena;
    FAIL_IF(
        fetch->size > M_MAX_JSON_SIZE, "%srepo.js is larger than %zu bytes",
        url, M_MAX_JSON_SIZE);
    uint8_t *const text = Arena_Alloc(arena, fetch->size);
    if (fetch->size > 0) {
        memcpy(text, fetch->data, fetch->size);
    }
    JSON_VALUE *root;
    MUST(Json_Parse(
        (const char *)text, fetch->size, Arena_Allocator(arena), &root));
    const char *const id = Json_GetString(Json_Get(root, "id"), nullptr);
    FAIL_IF(!M_IsSafeName(id), "%srepo.js has no valid id", url);
    if (M_KnownRepo(network, id) != nullptr) {
        return OK;
    }
    REPO *const repo = Arena_AllocZ(arena, sizeof(*repo));
    repo->id = id;
    repo->title = Json_GetString(Json_Get(root, "title"), id);
    repo->text = text;
    repo->size = fetch->size;
    const JSON_VALUE *const patches = Json_Get(root, "patches");
    repo->patches =
        patches != nullptr && patches->type == JSON_OBJECT ? patches : nullptr;

    const JSON_VALUE *const servers = Json_Get(root, "servers");
    const int32_t listed = servers != nullptr && servers->type == JSON_ARRAY
        ? servers->array.count
        : 0;
    repo->servers = Arena_AllocArr(arena, (size_t)listed + 1, sizeof(char *));
    repo->servers[repo->server_count++] = url;
    for (int32_t i = 0; i < listed; i++) {
        const char *const server = Json_GetString(Json_At(servers, i), nullptr);
        if (server == nullptr) {
            continue;
        }
        const char *const folder = M_FolderUrl(arena, server);
        if (strcmp(folder, url) != 0) {
            repo->servers[repo->server_count++] = folder;
        }
        M_Queue(network, folder);
    }
    const JSON_VALUE *const neighbors = Json_Get(root, "neighbors");
    for (int32_t i = 0; neighbors != nullptr && neighbors->type == JSON_ARRAY
         && i < neighbors->array.count;
         i++) {
        const char *const neighbor =
            Json_GetString(Json_At(neighbors, i), nullptr);
        if (neighbor != nullptr) {
            M_Queue(network, neighbor);
        }
    }
    Array_Append(&network->repos, repo);
    return OK;
}

// Fetches the next URLs of the walk, as many as run at the same time.
// Returns false when no URL is left.
static bool M_Walk(REPO_NETWORK *const network)
{
    const int32_t start = network->next;
    const int32_t count =
        MIN(network->urls.count - start, (int32_t)FETCH_MAX_TRANSFERS);
    if (count <= 0) {
        return false;
    }
    network->next += count;
    FETCH fetches[FETCH_MAX_TRANSFERS] = {};
    for (int32_t i = 0; i < count; i++) {
        fetches[i].url = M_Format(
            network->arena, "%srepo.js", network->urls.items[start + i]);
    }
    Fetch_Run(fetches, count);
    for (int32_t i = 0; i < count; i++) {
        if (IS_OK(fetches[i].result)) {
            Result_Ignore(M_AddRepo(
                network, network->urls.items[start + i], &fetches[i]));
        }
        Fetch_Release(&fetches[i]);
    }
    return true;
}

RESULT Repo_Open(
    const char *const url, ARENA *const arena, REPO_NETWORK **const out)
{
    *out = nullptr;
    REPO_NETWORK *const network = Arena_AllocZ(arena, sizeof(*network));
    network->arena = arena;
    Array_Init(&network->repos, Arena_Allocator(arena));
    Array_Init(&network->urls, Arena_Allocator(arena));
    Array_Init(&network->patches, Arena_Allocator(arena));

    const char *folder = M_FolderUrl(arena, url);
    if (Str_EndsWith(url, "repo.js")) {
        char *const trimmed = M_Dup(arena, url);
        trimmed[strlen(url) - strlen("repo.js")] = '\0';
        folder = trimmed;
    }
    Array_Append(&network->urls, folder);
    network->next = 1;
    FETCH fetch = { .url = M_Format(arena, "%srepo.js", folder) };
    Fetch_Run(&fetch, 1);
    if (!IS_OK(fetch.result)) {
        const RESULT result = fetch.result;
        fetch.result = OK;
        Fetch_Release(&fetch);
        return result;
    }
    const RESULT added = M_AddRepo(network, folder, &fetch);
    Fetch_Release(&fetch);
    MUST(added, "%srepo.js", folder);
    *out = network;
    return OK;
}

const REPO *Repo_GetFirst(const REPO_NETWORK *const network)
{
    return network->repos.items[0];
}

const REPO *Repo_FindRepo(REPO_NETWORK *const network, const char *const id)
{
    const REPO *repo;
    while ((repo = M_KnownRepo(network, id)) == nullptr) {
        if (!M_Walk(network)) {
            return nullptr;
        }
    }
    return repo;
}

bool Repo_HasPatch(const REPO *const repo, const char *const name)
{
    return Json_Get(repo->patches, name) != nullptr;
}

const REPO *Repo_FindPatch(
    REPO_NETWORK *const network, const REPO *const preferred,
    const char *const name)
{
    if (preferred != nullptr && Repo_HasPatch(preferred, name)) {
        return preferred;
    }
    int32_t checked = 0;
    for (;;) {
        for (; checked < network->repos.count; checked++) {
            if (Repo_HasPatch(network->repos.items[checked], name)) {
                return network->repos.items[checked];
            }
        }
        if (!M_Walk(network)) {
            return nullptr;
        }
    }
}

// Returns the patch, fetching its patch.js from the servers of its
// repository in order when it is not known yet.
static RESULT M_GetPatch(
    REPO_NETWORK *const network, const REPO *const repo, const char *const name,
    const REPO_PATCH **const out)
{
    *out = nullptr;
    for (int32_t i = 0; i < network->patches.count; i++) {
        const REPO_PATCH *const patch = network->patches.items[i];
        if (patch->repo == repo && strcmp(patch->name, name) == 0) {
            *out = patch;
            return OK;
        }
    }
    FAIL_IF(
        !M_IsSafeName(name), "%s/%s is not a valid patch id", repo->id, name);
    ARENA *const arena = network->arena;
    RESULT last = OK;
    FETCH fetch = {};
    for (int32_t i = 0; i < repo->server_count; i++) {
        Result_Ignore(last);
        fetch = (FETCH) {
            .url = M_Format(arena, "%s%s/patch.js", repo->servers[i], name),
        };
        Fetch_Run(&fetch, 1);
        last = fetch.result;
        fetch.result = OK;
        if (IS_OK(last)) {
            break;
        }
        Fetch_Release(&fetch);
    }
    MUST(last);
    if (fetch.size > M_MAX_JSON_SIZE) {
        Fetch_Release(&fetch);
        return FAIL(
            "patch.js of %s/%s is larger than %zu bytes", repo->id, name,
            M_MAX_JSON_SIZE);
    }

    REPO_PATCH *const patch = Arena_AllocZ(arena, sizeof(*patch));
    patch->repo = repo;
    patch->name = M_Dup(arena, name);
    patch->id = M_Format(arena, "%s/%s", repo->id, name);
    uint8_t *const text = Arena_Alloc(arena, fetch.size);
    if (fetch.size > 0) {
        memcpy(text, fetch.data, fetch.size);
    }
    patch->text = text;
    patch->size = fetch.size;
    Fetch_Release(&fetch);
    JSON_VALUE *root;
    MUST(
        Json_Parse(
            (const char *)text, patch->size, Arena_Allocator(arena), &root),
        "patch.js of %s", patch->id);
    FAIL_IF(
        root->type != JSON_OBJECT, "patch.js of %s is not an object",
        patch->id);
    patch->json = root;
    patch->title = Json_GetString(Json_Get(root, "title"), patch->id);

    const JSON_VALUE *const servers = Json_Get(root, "servers");
    const int32_t listed = servers != nullptr && servers->type == JSON_ARRAY
        ? servers->array.count
        : 0;
    patch->servers = Arena_AllocArr(
        arena, (size_t)listed + (size_t)repo->server_count, sizeof(char *));
    for (int32_t i = 0; i < listed; i++) {
        const char *const server = Json_GetString(Json_At(servers, i), nullptr);
        if (server != nullptr) {
            patch->servers[patch->server_count++] = M_FolderUrl(arena, server);
        }
    }
    for (int32_t i = 0; i < repo->server_count; i++) {
        patch->servers[patch->server_count++] =
            M_Format(arena, "%s%s/", repo->servers[i], name);
    }
    Array_Append(&network->patches, patch);
    *out = patch;
    return OK;
}

static bool M_Contains(
    const REPO_PATCH_LIST *const list, const REPO_PATCH *const patch)
{
    for (int32_t i = 0; i < list->count; i++) {
        if (list->items[i] == patch) {
            return true;
        }
    }
    return false;
}

// Resolves id as Repo_AddPatch describes it, with context as the
// repository that a bare id prefers.
static RESULT M_Resolve(
    REPO_NETWORK *const network, const char *const id,
    const REPO *const context, const REPO_PATCH **const out)
{
    *out = nullptr;
    const char *const slash = strchr(id, '/');
    if (slash != nullptr) {
        char *const repo_id = M_Dup(network->arena, id);
        repo_id[slash - id] = '\0';
        const REPO *const repo = Repo_FindRepo(network, repo_id);
        FAIL_IF(
            repo == nullptr, "no repository in the network has the id %s",
            repo_id);
        FAIL_IF(
            !Repo_HasPatch(repo, slash + 1), "repository %s has no patch %s",
            repo_id, slash + 1);
        return M_GetPatch(network, repo, slash + 1, out);
    }
    const REPO *const repo = Repo_FindPatch(network, context, id);
    FAIL_IF(repo == nullptr, "no repository in the network has a patch %s", id);
    return M_GetPatch(network, repo, id, out);
}

static RESULT M_Add(
    REPO_NETWORK *const network, const REPO_PATCH *const patch,
    REPO_PATCH_LIST *const stack, REPO_PATCH_LIST *const list)
{
    if (M_Contains(list, patch) || M_Contains(stack, patch)) {
        return OK;
    }
    Array_Append(stack, patch);
    const JSON_VALUE *const dependencies =
        Json_Get(patch->json, "dependencies");
    for (int32_t i = 0; dependencies != nullptr
         && dependencies->type == JSON_ARRAY && i < dependencies->array.count;
         i++) {
        const char *const id =
            Json_GetString(Json_At(dependencies, i), nullptr);
        if (id == nullptr) {
            continue;
        }
        const REPO_PATCH *dependency;
        MUST(
            M_Resolve(network, id, patch->repo, &dependency),
            "dependency %s of %s", id, patch->id);
        MUST(M_Add(network, dependency, stack, list));
    }
    stack->count--;
    Array_Append(list, patch);
    return OK;
}

RESULT Repo_AddPatch(
    REPO_NETWORK *const network, const char *const id,
    REPO_PATCH_LIST *const list)
{
    const REPO_PATCH *patch;
    MUST(M_Resolve(network, id, Repo_GetFirst(network), &patch));
    const int32_t old_count = list->count;
    REPO_PATCH_LIST stack;
    Array_Init(&stack, Arena_Allocator(network->arena));
    const RESULT result = M_Add(network, patch, &stack, list);
    if (!IS_OK(result)) {
        list->count = old_count;
    }
    return result;
}
