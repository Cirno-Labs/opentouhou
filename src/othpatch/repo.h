// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "core/base/arena.h"
#include "core/base/array.h"
#include "core/base/result.h"
#include "core/base/types.h"
#include "core/json/types.h"

// Finds patches in a network of thcrap repositories, with the patches that
// they depend on.
//
// A repository is a folder on a server with a repo.js: its id, its title,
// servers (the URLs of copies of the folder), neighbors (the URLs of other
// repositories) and patches (an object from patch name to title). The
// patch "<name>" of a repository is the folder <server><name>/ with a
// patch.js, and has the id "<repository id>/<name>".
//
// The network is walked breadth-first from its first repository, through
// the servers and the neighbors of each repo.js, and only as far as a
// lookup needs. A repository that cannot be fetched or parsed is skipped,
// and so is a repository whose id is known already. Ids and patch names
// become folder names, so one that is empty, "." or "..", or has a control
// character or one of / \ : * ? " < > | is skipped too.

// Holds one repository.
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
} REPO;

// Holds one patch.
typedef struct {
    const REPO *repo;
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
} REPO_PATCH;

typedef ARRAY(const REPO_PATCH *) REPO_PATCH_LIST;

typedef struct REPO_NETWORK REPO_NETWORK;

// Fetches the first repository of a network: the repo.js at url when url
// ends with "repo.js", or at <url>/repo.js. Every allocation of the
// network and of what it returns comes from arena, so that releasing the
// arena releases the network. A repo.js or patch.js larger than 1 MiB
// counts as one that cannot be fetched.
RESULT Repo_Open(const char *url, ARENA *arena, REPO_NETWORK **out);

// Returns the first repository of a network.
const REPO *Repo_GetFirst(const REPO_NETWORK *network);

// Returns the repository with an id, or nullptr when the network has none.
const REPO *Repo_FindRepo(REPO_NETWORK *network, const char *id);

// Returns the repository that has the patch name: preferred when it has
// it, or else the first repository in the order of the walk that has it,
// or nullptr when none has it. preferred may be nullptr.
const REPO *Repo_FindPatch(
    REPO_NETWORK *network, const REPO *preferred, const char *name);

// Returns whether the patches of a repository list name.
bool Repo_HasPatch(const REPO *repo, const char *name);

// Adds the patch id to list, after each patch that it depends on, unless
// the list holds them already. id is "<repository id>/<name>", or a bare
// "<name>" that the first repository that has it gives, the first
// repository of the network first. The dependencies of patch.js resolve
// in the same way, but a bare one prefers the repository of the patch that
// names it, as thcrap's configurator does. A dependency that leads back to
// a patch that is being added is left out. On failure, such as for a patch
// or a dependency that the network does not have, the list is as it was.
RESULT Repo_AddPatch(
    REPO_NETWORK *network, const char *id, REPO_PATCH_LIST *list);
