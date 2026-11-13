#pragma once

#include "core/base/arena.h"
#include "core/base/array.h"
#include "core/base/result.h"
#include "core/base/types.h"
#include "core/json/types.h"
#include "core/patch/fetch.h"
#include "core/patch/manager.h"

// Walks the network of thcrap repositories that manager.h describes, and
// finds patches in it with the patches that they depend on. Ids and patch
// names become folder names, so one that is empty, "." or "..", or has a
// control character or one of / \ : * ? " < > | is skipped.

// Holds one repository of the network.
typedef struct {
    const char *id;
    // Holds the title, or the id when repo.js has none.
    const char *title;
    // Holds the URLs of the repository, each ending with '/': the one that
    // repo.js came from first, then the servers that repo.js lists.
    const char **servers;
    int32_t server_count;
    // Holds the patches member of repo.js, or nullptr when it has none.
    const JSON_VALUE *patches;
    // Holds repo.js as it was fetched.
    const uint8_t *text;
    size_t size;
} PATCH_REPO;

// Holds one patch.
typedef struct {
    const PATCH_REPO *repo;
    const char *name;
    const char *id;
    // Holds the title from patch.js, or the id when patch.js has none.
    const char *title;
    // Holds patch.js, parsed and as it was fetched.
    const JSON_VALUE *json;
    const uint8_t *text;
    size_t size;
    // Holds the URLs where the files of the patch are, each ending with
    // '/': the servers that patch.js lists, then <server><name>/ for each
    // server of the repository.
    const char **servers;
    int32_t server_count;
} PATCH_REMOTE;

typedef ARRAY(const PATCH_REMOTE *) PATCH_REMOTE_LIST;

typedef struct PATCH_NETWORK PATCH_NETWORK;

// Fetches the first repository of a network with fetch: the repo.js at
// url when url ends with "repo.js", or at <url>/repo.js. Every allocation of
// the network and of what it returns comes from arena, so that releasing the
// arena releases the network. A repo.js or patch.js larger than 1 MiB
// counts as one that cannot be fetched.
RESULT PatchRepo_Open(
    const char *url, PATCH_FETCH_FN fetch, ARENA *arena, PATCH_NETWORK **out);

// Returns the first repository of a network.
const PATCH_REPO *PatchRepo_GetFirst(const PATCH_NETWORK *network);

// Returns the repository with an id, or nullptr when the network has none.
const PATCH_REPO *PatchRepo_FindRepo(PATCH_NETWORK *network, const char *id);

// Returns the repository that has the patch name: preferred when it has
// it, or else the first repository in the order of the walk that has it,
// or nullptr when none has it. preferred may be nullptr.
const PATCH_REPO *PatchRepo_FindPatch(
    PATCH_NETWORK *network, const PATCH_REPO *preferred, const char *name);

// Walks the whole network and returns the number of repositories that have
// the patch name. Writes the first capacity of them into repos, in the
// order of the walk.
int32_t PatchRepo_FindAllPatch(
    PATCH_NETWORK *network, const char *name, const PATCH_REPO **repos,
    int32_t capacity);

// Returns whether the patches of a repository list name.
bool PatchRepo_HasPatch(const PATCH_REPO *repo, const char *name);

// Adds the patch id to list, after each patch that it depends on, unless
// the list holds them already. id is "<repository id>/<name>", or a bare
// "<name>" that the first repository that has it gives, the first
// repository of the network first. The dependencies of patch.js resolve
// in the same way, but a bare one prefers the repository of the patch that
// names it, as thcrap's configurator does. A dependency that leads back to
// a patch that is being added is left out. On failure, such as for a patch
// or a dependency that the network does not have, the list is as it was.
RESULT PatchRepo_AddPatch(
    PATCH_NETWORK *network, const char *id, PATCH_REMOTE_LIST *list);
