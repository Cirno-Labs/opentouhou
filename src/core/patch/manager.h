#pragma once

#include "core/base/result.h"
#include "core/base/types.h"
#include "core/json/types.h"
#include "core/patch/fetch.h"
#include "core/patch/select.h"

// Installs, updates and removes thcrap patches in a patches folder, from a
// network of thcrap repositories.
//
// The patches folder has the layout of thcrap's repos/ folder that
// Patch_Select reads: <dir>/<repository id>/repo.js, and
// <dir>/<repository id>/<patch name>/ with the patch.js, the files.js and
// the files of the patch. Of the files that files.js lists, a patch keeps
// the files that every game shares, whose paths have no '/', and the files
// of the games that PatchManager_Open names, whose paths start with
// "<game>/".
//
// A repository is a folder on a server with a repo.js: its id, its title,
// servers (the URLs of copies of the folder), neighbors (the URLs of other
// repositories) and patches (an object from patch name to title). The
// patch "<name>" of a repository is the folder <server><name>/ with a
// patch.js, and has the id "<repository id>/<name>". The network is walked
// breadth-first from repo_url, and only as far as a call needs; the first
// call that needs it fetches the first repository. A repository that
// cannot be fetched or parsed is skipped, and so is a repository whose id
// is known already, or whose id or patch names are not safe folder names.
//
// The update of a patch fetches each file whose CRC-32 does not match
// files.js, with "?crc32=<hex>" after its URL, from each server of the
// patch in order until one gives it with that CRC-32. When every file has
// arrived, it moves the files into place, removes the files that files.js
// marks as deleted with null, and writes repo.js, files.js and patch.js
// last. When a file does not arrive, the update reports failure and
// changes no file of the patch.

typedef struct PATCH_MANAGER PATCH_MANAGER;

// Holds a patch that a repository lists: its id, "<repository id>/<name>",
// and the title that repo.js gives it, or "" when it gives none.
typedef struct {
    const char *id;
    const char *title;
} PATCH_LISTED;

// Holds what the update of one patch did to its files.
typedef struct {
    int32_t downloaded;
    int32_t up_to_date;
    int32_t removed;
} PATCH_COUNTS;

// Reports what an install, an update or a removal does. patch_queued
// comes for each patch that an install or an update brings up to date,
// before the call fetches a file of any patch. patch_started comes when
// the update of a patch starts to fetch files, with the number of files
// that it fetches, and only when that is more than 0. file_progress comes
// as the bytes of a file arrive: received of size, where size is 0 while
// it is unknown. file_done comes when a file has arrived with the right
// CRC-32. patch_done comes when the update of a patch ends with success,
// with what it did. patch_failed comes instead when the update of a patch
// fails, with the reason, and the call then reports failure. patch_kept
// comes when an update leaves an installed patch as it is because the
// network cannot give it. patch_removed comes for each patch that a
// removal removes.
//
// file is the path of the file in the patch, such as "th06/msg1.dat.jdiff".
// A patch of these events is an entry of the network, which holds the id,
// the title, the dir and patch_js only. Each callback may be nullptr.
typedef struct {
    void *user;
    void (*patch_queued)(void *user, const PATCH_ENTRY *patch);
    void (*patch_started)(void *user, const PATCH_ENTRY *patch, int32_t files);
    void (*file_progress)(
        void *user, const PATCH_ENTRY *patch, const char *file, size_t received,
        size_t size);
    void (*file_done)(void *user, const PATCH_ENTRY *patch, const char *file);
    void (*patch_done)(
        void *user, const PATCH_ENTRY *patch, const PATCH_COUNTS *counts);
    void (*patch_failed)(
        void *user, const PATCH_ENTRY *patch, const char *reason);
    void (*patch_kept)(void *user, const char *id, const char *reason);
    void (*patch_removed)(void *user, const char *id);
} PATCH_EVENTS;

// Opens the patches folder dir for the games that games names, such as "th06".
// fetch gets every file of the network that starts at repo_url: the repo.js at
// repo_url when it ends with "repo.js", or else at <repo_url>/repo.js. A folder
// that cannot be read has no patches.
PATCH_MANAGER *PatchManager_Open(
    const char *dir, const char *repo_url, const char *const *games,
    int32_t game_count, PATCH_FETCH_FN fetch);

// Returns the installed patches, in byte order of the ids, as
// Patch_GetEntry does for a selection of every game. An install, an update
// and a removal read them again. An index out of range is fatal.
int32_t PatchManager_GetCount(const PATCH_MANAGER *manager);
const PATCH_ENTRY *PatchManager_Get(const PATCH_MANAGER *manager, int32_t i);

// Lists the patches of the first repository of the network, in byte order
// of the ids. The list stays valid until the manager closes.
RESULT PatchManager_ListPatches(
    PATCH_MANAGER *manager, const PATCH_LISTED **out, int32_t *count);

// Installs the patch name and every patch that it needs, the patch first,
// and brings each up to date, and then every other installed patch as
// Update does. name is "<repository id>/<patch name>", or a bare
// "<patch name>" that one repository of the network has. A dependency that
// patch.js names bare prefers the repository of the patch that names it,
// as thcrap's configurator does. Patches that need each other in a loop
// install once each. Stops at the first patch that fails.
//
// The message of a failure is one of:
//
// - "could not install <id>: <reason>" when the patch or a patch that it
//   needs is not in the network. <id> is the id of the patch, or name as
//   it is when no repository has it.
// - "<count> repositories have a patch <name>: <id>, <id>. Name one as
//   <repo>/<patch>" for a bare name that several repositories have.
// - "<title> (<id>) has no files for <program>" for a language pack whose
//   files.js lists no file below "<game>/" for any of the games, where
//   <program> is "o<game>" for each game, separated by ", ". The title and
//   the parentheses are left out when the title is the id.
// - "could not download <id>: <reason>" when a fetch fails, after
//   patch_failed for the patch <id> with <reason>.
//
// The first three fetch no file of a patch and change nothing in dir.
RESULT PatchManager_Install(
    PATCH_MANAGER *manager, const char *name, const PATCH_EVENTS *events);

// Brings every installed patch up to date, each after the patches that it
// needs, except the patches whose patch.js has "update": false and the
// patches that an install or an update of this manager brought up to date
// already. Fetches nothing when no installed patch updates. Stops at the
// first patch that fails, with the message "could not download <id>:
// <reason>" after patch_failed.
RESULT PatchManager_Update(PATCH_MANAGER *manager, const PATCH_EVENTS *events);

// Removes the installed patch id and the patches that only it needs, but
// never a language pack. Fails, and removes nothing, when another
// installed patch needs id. A removal deletes the files that files.js
// lists, with their .part files, files.js and patch.js, and then each
// folder of the patch that is empty. Other files stay.
RESULT PatchManager_Remove(
    PATCH_MANAGER *manager, const char *id, const PATCH_EVENTS *events);

// Closes a manager. A nullptr manager does nothing.
void PatchManager_Close(PATCH_MANAGER *manager);
