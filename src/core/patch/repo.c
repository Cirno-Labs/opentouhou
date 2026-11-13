#include "core/patch/repo.h"

#include "core/base/string.h"
#include "core/json/get.h"
#include "core/json/parse.h"

#include <stdarg.h>
#include <string.h>

// Sets the largest repo.js and patch.js that the walk accepts.
#define M_MAX_JSON_SIZE ((size_t)1024 * 1024)

// Sets how many repo.js files the walk fetches at the same time.
#define M_WALK_BATCH 8

typedef ARRAY(const char *) M_URL_LIST;
typedef ARRAY(PATCH_REPO *) M_REPO_LIST;
typedef ARRAY(PATCH_REMOTE *) M_PATCH_LIST;

struct PATCH_NETWORK {
    ARENA *arena;
    PATCH_FETCH_FN fetch;
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

static void M_Queue(PATCH_NETWORK *const network, const char *const url)
{
    const char *const folder = M_FolderUrl(network->arena, url);
    for (int32_t i = 0; i < network->urls.count; i++) {
        if (strcmp(network->urls.items[i], folder) == 0) {
            return;
        }
    }
    Array_Append(&network->urls, folder);
}

static const PATCH_REPO *M_KnownRepo(
    const PATCH_NETWORK *const network, const char *const id)
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
    PATCH_NETWORK *const network, const char *const url,
    const PATCH_FETCH *const fetch)
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
    PATCH_REPO *const repo = Arena_AllocZ(arena, sizeof(*repo));
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
static bool M_Walk(PATCH_NETWORK *const network)
{
    const int32_t start = network->next;
    const int32_t count = MIN(network->urls.count - start, M_WALK_BATCH);
    if (count <= 0) {
        return false;
    }
    network->next += count;
    PATCH_FETCH fetches[M_WALK_BATCH] = {};
    for (int32_t i = 0; i < count; i++) {
        fetches[i].url = M_Format(
            network->arena, "%srepo.js", network->urls.items[start + i]);
    }
    network->fetch(fetches, count, nullptr);
    for (int32_t i = 0; i < count; i++) {
        if (IS_OK(fetches[i].result)) {
            Result_Ignore(M_AddRepo(
                network, network->urls.items[start + i], &fetches[i]));
        }
        PatchFetch_Release(&fetches[i]);
    }
    return true;
}

RESULT PatchRepo_Open(
    const char *const url, const PATCH_FETCH_FN fetch, ARENA *const arena,
    PATCH_NETWORK **const out)
{
    *out = nullptr;
    PATCH_NETWORK *const network = Arena_AllocZ(arena, sizeof(*network));
    network->arena = arena;
    network->fetch = fetch;
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
    PATCH_FETCH repo_js = { .url = M_Format(arena, "%srepo.js", folder) };
    fetch(&repo_js, 1, nullptr);
    if (!IS_OK(repo_js.result)) {
        const RESULT result = repo_js.result;
        repo_js.result = OK;
        PatchFetch_Release(&repo_js);
        return result;
    }
    const RESULT added = M_AddRepo(network, folder, &repo_js);
    PatchFetch_Release(&repo_js);
    MUST(added, "%srepo.js", folder);
    *out = network;
    return OK;
}

const PATCH_REPO *PatchRepo_GetFirst(const PATCH_NETWORK *const network)
{
    return network->repos.items[0];
}

const PATCH_REPO *PatchRepo_FindRepo(
    PATCH_NETWORK *const network, const char *const id)
{
    const PATCH_REPO *repo;
    while ((repo = M_KnownRepo(network, id)) == nullptr) {
        if (!M_Walk(network)) {
            return nullptr;
        }
    }
    return repo;
}

bool PatchRepo_HasPatch(const PATCH_REPO *const repo, const char *const name)
{
    return Json_Get(repo->patches, name) != nullptr;
}

const PATCH_REPO *PatchRepo_FindPatch(
    PATCH_NETWORK *const network, const PATCH_REPO *const preferred,
    const char *const name)
{
    if (preferred != nullptr && PatchRepo_HasPatch(preferred, name)) {
        return preferred;
    }
    int32_t checked = 0;
    for (;;) {
        for (; checked < network->repos.count; checked++) {
            if (PatchRepo_HasPatch(network->repos.items[checked], name)) {
                return network->repos.items[checked];
            }
        }
        if (!M_Walk(network)) {
            return nullptr;
        }
    }
}

int32_t PatchRepo_FindAllPatch(
    PATCH_NETWORK *const network, const char *const name,
    const PATCH_REPO **const repos, const int32_t capacity)
{
    while (M_Walk(network)) {}
    int32_t count = 0;
    for (int32_t i = 0; i < network->repos.count; i++) {
        if (PatchRepo_HasPatch(network->repos.items[i], name)) {
            if (count < capacity) {
                repos[count] = network->repos.items[i];
            }
            count++;
        }
    }
    return count;
}

// Returns the patch, fetching its patch.js from the servers of its
// repository in order when it is not known yet.
static RESULT M_GetPatch(
    PATCH_NETWORK *const network, const PATCH_REPO *const repo,
    const char *const name, const PATCH_REMOTE **const out)
{
    *out = nullptr;
    for (int32_t i = 0; i < network->patches.count; i++) {
        const PATCH_REMOTE *const patch = network->patches.items[i];
        if (patch->repo == repo && strcmp(patch->name, name) == 0) {
            *out = patch;
            return OK;
        }
    }
    FAIL_IF(
        !M_IsSafeName(name), "%s/%s is not a valid patch id", repo->id, name);
    ARENA *const arena = network->arena;
    RESULT last = OK;
    PATCH_FETCH fetch = {};
    for (int32_t i = 0; i < repo->server_count; i++) {
        Result_Ignore(last);
        fetch = (PATCH_FETCH) {
            .url = M_Format(arena, "%s%s/patch.js", repo->servers[i], name),
        };
        network->fetch(&fetch, 1, nullptr);
        last = fetch.result;
        fetch.result = OK;
        if (IS_OK(last)) {
            break;
        }
        PatchFetch_Release(&fetch);
    }
    MUST(last);
    if (fetch.size > M_MAX_JSON_SIZE) {
        PatchFetch_Release(&fetch);
        return FAIL(
            "patch.js of %s/%s is larger than %zu bytes", repo->id, name,
            M_MAX_JSON_SIZE);
    }

    PATCH_REMOTE *const patch = Arena_AllocZ(arena, sizeof(*patch));
    patch->repo = repo;
    patch->name = M_Dup(arena, name);
    patch->id = M_Format(arena, "%s/%s", repo->id, name);
    uint8_t *const text = Arena_Alloc(arena, fetch.size);
    if (fetch.size > 0) {
        memcpy(text, fetch.data, fetch.size);
    }
    patch->text = text;
    patch->size = fetch.size;
    PatchFetch_Release(&fetch);
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
    const PATCH_REMOTE_LIST *const list, const PATCH_REMOTE *const patch)
{
    for (int32_t i = 0; i < list->count; i++) {
        if (list->items[i] == patch) {
            return true;
        }
    }
    return false;
}

// Resolves id as PatchRepo_AddPatch describes it, with context as the
// repository that a bare id prefers.
static RESULT M_Resolve(
    PATCH_NETWORK *const network, const char *const id,
    const PATCH_REPO *const context, const PATCH_REMOTE **const out)
{
    *out = nullptr;
    const char *const slash = strchr(id, '/');
    if (slash != nullptr) {
        char *const repo_id = M_Dup(network->arena, id);
        repo_id[slash - id] = '\0';
        const PATCH_REPO *const repo = PatchRepo_FindRepo(network, repo_id);
        FAIL_IF(
            repo == nullptr, "no repository in the network has the id %s",
            repo_id);
        FAIL_IF(
            !PatchRepo_HasPatch(repo, slash + 1),
            "repository %s has no patch %s", repo_id, slash + 1);
        return M_GetPatch(network, repo, slash + 1, out);
    }
    const PATCH_REPO *const repo = PatchRepo_FindPatch(network, context, id);
    FAIL_IF(repo == nullptr, "no repository in the network has a patch %s", id);
    return M_GetPatch(network, repo, id, out);
}

static RESULT M_Add(
    PATCH_NETWORK *const network, const PATCH_REMOTE *const patch,
    PATCH_REMOTE_LIST *const stack, PATCH_REMOTE_LIST *const list)
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
        const PATCH_REMOTE *dependency;
        MUST(
            M_Resolve(network, id, patch->repo, &dependency),
            "dependency %s of %s", id, patch->id);
        MUST(M_Add(network, dependency, stack, list));
    }
    stack->count--;
    Array_Append(list, patch);
    return OK;
}

RESULT PatchRepo_AddPatch(
    PATCH_NETWORK *const network, const char *const id,
    PATCH_REMOTE_LIST *const list)
{
    const PATCH_REMOTE *patch;
    MUST(M_Resolve(network, id, PatchRepo_GetFirst(network), &patch));
    const int32_t old_count = list->count;
    PATCH_REMOTE_LIST stack;
    Array_Init(&stack, Arena_Allocator(network->arena));
    const RESULT result = M_Add(network, patch, &stack, list);
    if (!IS_OK(result)) {
        list->count = old_count;
    }
    return result;
}
