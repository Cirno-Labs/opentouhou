#include "othpatch/context.h"

#include "othpatch/fetch.h"
#include "othpatch/game.h"
#include "othpatch/install.h"
#include "othpatch/release.h"

#include "core/base/string.h"
#include "core/hash/sha256.h"
#include "core/io/file.h"
#include "core/json/parse.h"

#include <ctype.h>
#include <string.h>

// Holds the state of the update checks of one run.
typedef struct {
    APP_CONTEXT *run;
    const INSTALL_PLATFORM *platform;
    // Holds the version of the othpatch in exe_dir: the running one, or
    // the one that an update installed. othpatch_is_release is false for a
    // development build, which is never replaced.
    RELEASE_VERSION othpatch_version;
    bool othpatch_is_release;
    // Holds whether the release index was fetched, and the index, or
    // nullptr with the reason when the fetch failed.
    bool releases_fetched;
    JSON_VALUE *releases;
    const char *releases_error;
} M_UPDATE;

static RESULT M_Fetch(
    const char *const url, const PATCH_FETCH_PROGRESS *const progress,
    PATCH_FETCH *const fetch)
{
    *fetch = (PATCH_FETCH) { .url = url };
    Fetch_Run(fetch, 1, progress);
    const RESULT result = fetch->result;
    fetch->result = OK;
    if (!IS_OK(result)) {
        PatchFetch_Release(fetch);
    }
    return result;
}

static void M_FormatVersion(
    const RELEASE_VERSION version, char out[GAME_VERSION_SIZE])
{
    snprintf(
        out, GAME_VERSION_SIZE, "%d.%d.%d", version.major, version.minor,
        version.patch);
}

// Downloads the package of release and checks its SHA-256 digest. name is
// the program that it holds. PatchFetch_Release releases *out.
static RESULT M_Download(
    const M_UPDATE *const update, const char *const name,
    const RELEASE *const release, PATCH_FETCH *const out)
{
    FAIL_IF(
        release->sha256 == nullptr, "the index gives no SHA-256 digest of %s",
        release->package);
    char newest[GAME_VERSION_SIZE];
    M_FormatVersion(release->version, newest);
    CONSOLE_BAR bar = {
        .run = update->run,
        .label = Console_Format(update->run->arena, "%s %s", name, newest),
    };
    const PATCH_FETCH_PROGRESS progress = {
        .received = Console_ReceiveBytes,
        .context = &bar,
    };
    const RESULT fetched = M_Fetch(release->url, &progress, out);
    if (IS_OK(fetched) || bar.received > 0) {
        Console_EndBytes(&bar, IS_OK(fetched));
    }
    MUST(fetched, "%s", release->package);
    char digest[SHA256_HEX_SIZE];
    Sha256_ComputeHex(out->data, out->size, digest);
    if (!Str_EqualIgnoreCase(digest, release->sha256)) {
        PatchFetch_Release(out);
        return FAIL(
            "%s has the SHA-256 digest %s instead of %s", release->package,
            digest, release->sha256);
    }
    return OK;
}

// Returns whether the answer on in, one line, is yes. An empty line is yes.
static bool M_ReadYes(FILE *const in)
{
    char line[64];
    if (fgets(line, sizeof(line), in) == nullptr) {
        return false;
    }
    char *end = line + strlen(line);
    while (end > line && isspace((unsigned char)end[-1])) {
        *--end = '\0';
    }
    const char *start = line;
    while (isspace((unsigned char)*start)) {
        start++;
    }
    return start[0] == '\0' || Str_EqualIgnoreCase(start, "y")
        || Str_EqualIgnoreCase(start, "yes");
}

// Returns the release index, and fetches it on the first call. Prints the
// reason and returns nullptr when the fetch fails.
static const JSON_VALUE *M_GetReleases(M_UPDATE *const update)
{
    APP_CONTEXT *const run = update->run;
    if (!update->releases_fetched) {
        update->releases_fetched = true;
        PATCH_FETCH fetch;
        RESULT fetched = M_Fetch(run->args->releases, nullptr, &fetch);
        if (IS_OK(fetched)) {
            fetched = Json_Parse(
                (const char *)fetch.data, fetch.size,
                Arena_Allocator(run->arena), &update->releases);
            PatchFetch_Release(&fetch);
        }
        if (!IS_OK(fetched)) {
            update->releases = nullptr;
            update->releases_error =
                Console_Format(run->arena, "%s", fetched.message);
            Result_Ignore(fetched);
        }
    }
    if (update->releases == nullptr) {
        fprintf(
            run->out, "Could not check for updates: %s.\n",
            update->releases_error);
    }
    return update->releases;
}

// Finds the newest release of program, such as "oth06" or "othpatch", in
// the release index and compares it with installed. Returns whether the
// release is newer and has a package, and otherwise prints how the check
// ended. *failed tells whether the check failed.
static bool M_FindUpdate(
    M_UPDATE *const update, const char *const program,
    const RELEASE_VERSION installed, const char *const version,
    RELEASE *const out, bool *const failed)
{
    FILE *const stream = update->run->out;
    *failed = false;
    const JSON_VALUE *const index = M_GetReleases(update);
    if (index == nullptr) {
        *failed = true;
        return false;
    }
    const RESULT found =
        Release_FindNewest(index, program, update->platform->name, out);
    if (!IS_OK(found)) {
        Console_Fail(
            stream, found, "Could not check for updates: %s.", found.message);
        *failed = true;
        return false;
    }
    if (Release_CompareVersions(out->version, installed) <= 0) {
        fprintf(
            stream, "You already have the newest %s (%s).\n", program, version);
        return false;
    }
    char newest[GAME_VERSION_SIZE];
    M_FormatVersion(out->version, newest);
    if (out->url == nullptr) {
        fprintf(
            stream, "%s %s is out, but not for %s.\n", program, newest,
            update->platform->name);
        return false;
    }
    fprintf(stream, "%s %s is out (you have %s).\n", program, newest, version);
    return true;
}

// Returns whether to install a release: with --update, or when the answer
// on in is yes. Prints how the check ended otherwise.
static bool M_AskInstall(
    const M_UPDATE *const update, const char *const name,
    const char *const version)
{
    APP_CONTEXT *const run = update->run;
    if (run->args->update) {
        return true;
    }
    if (run->in == nullptr) {
        fprintf(
            run->out, "Kept %s %s. Run othpatch --update to install it.\n",
            name, version);
        return false;
    }
    fputs("Install it now? [Y/n] ", run->out);
    fflush(run->out);
    if (M_ReadYes(run->in)) {
        return true;
    }
    fprintf(
        run->out, "Kept %s %s. Run othpatch again to update.\n", name, version);
    return false;
}

static void M_PrintFailedInstall(
    const M_UPDATE *const update, const char *const name,
    const RELEASE *const release, const RESULT result)
{
    char newest[GAME_VERSION_SIZE];
    M_FormatVersion(release->version, newest);
    Console_Fail(
        update->run->out, result, "Could not update %s to %s: %s.", name,
        newest, result.message);
}

static void M_PrintOthpatchUpdated(
    FILE *const out, const char *const from, const RELEASE_VERSION to)
{
    char newest[GAME_VERSION_SIZE];
    M_FormatVersion(to, newest);
    fprintf(
        out, "Updated othpatch from %s to %s. The next run uses the new one.\n",
        from, newest);
}

static RESULT M_InstallGame(
    M_UPDATE *const update, const char *const game, const char *const name,
    const RELEASE *const release, bool *const othpatch_replaced)
{
    *othpatch_replaced = false;
    PATCH_FETCH fetch;
    MUST(M_Download(update, name, release, &fetch));
    const RESULT result = Install_Package(
        update->platform, update->run->exe_dir, game, fetch.data, fetch.size,
        update->othpatch_is_release ? &update->othpatch_version : nullptr,
        update->run->arena, othpatch_replaced);
    PatchFetch_Release(&fetch);
    return result;
}

// Runs the section of one game: checks for a newer release, and installs
// it when --update or the answer on in says so. Returns whether the
// section ended well.
static bool M_UpdateGame(
    M_UPDATE *const update, const char *const game, const char *const path)
{
    FILE *const out = update->run->out;
    char name[GAME_NAME_SIZE];
    Game_GetProductName(game, name);
    Console_StartSection(update->run, "Checking for %s updates...", name);
    char version[GAME_VERSION_SIZE];
    const RESULT asked = Game_GetVersion(path, version);
    if (!IS_OK(asked)) {
        Console_Fail(
            out, asked, "Could not read the version of %s: %s.", name,
            asked.message);
        return false;
    }
    RELEASE_VERSION installed;
    if (!Release_ParseVersion(version, &installed)) {
        fprintf(
            out,
            "%s %s is a development build, so othpatch does not update it.\n",
            name, version);
        return true;
    }
    RELEASE release;
    bool failed;
    if (!M_FindUpdate(update, name, installed, version, &release, &failed)) {
        return !failed;
    }
    if (!M_AskInstall(update, name, version)) {
        return true;
    }
    char othpatch[GAME_VERSION_SIZE];
    M_FormatVersion(update->othpatch_version, othpatch);
    bool othpatch_replaced;
    const RESULT result =
        M_InstallGame(update, game, name, &release, &othpatch_replaced);
    if (!IS_OK(result)) {
        M_PrintFailedInstall(update, name, &release, result);
        return false;
    }
    char newest[GAME_VERSION_SIZE];
    M_FormatVersion(release.version, newest);
    fprintf(out, "Updated %s from %s to %s.\n", name, version, newest);
    if (othpatch_replaced) {
        M_PrintOthpatchUpdated(out, othpatch, update->othpatch_version);
    }
    return true;
}

static RESULT M_InstallOthpatch(
    const M_UPDATE *const update, const RELEASE *const release)
{
    PATCH_FETCH fetch;
    MUST(M_Download(update, "othpatch", release, &fetch));
    const RESULT result = Install_Othpatch(
        update->platform, update->run->exe_dir, fetch.data, fetch.size,
        update->run->arena);
    PatchFetch_Release(&fetch);
    return result;
}

// Runs the section of othpatch: checks for a newer release, and installs
// it when --update or the answer on in says so. Returns whether the
// section ended well.
static bool M_UpdateOthpatch(M_UPDATE *const update, const char *const running)
{
    FILE *const out = update->run->out;
    Console_StartSection(update->run, "Checking for othpatch updates...");
    if (!update->othpatch_is_release) {
        fprintf(
            out,
            "othpatch %s is a development build, so othpatch does not update "
            "it.\n",
            running);
        return true;
    }
    char version[GAME_VERSION_SIZE];
    M_FormatVersion(update->othpatch_version, version);
    RELEASE release;
    bool failed;
    if (!M_FindUpdate(
            update, "othpatch", update->othpatch_version, version, &release,
            &failed)) {
        return !failed;
    }
    if (!M_AskInstall(update, "othpatch", version)) {
        return true;
    }
    const RESULT result = M_InstallOthpatch(update, &release);
    if (!IS_OK(result)) {
        M_PrintFailedInstall(update, "othpatch", &release, result);
        return false;
    }
    update->othpatch_version = release.version;
    M_PrintOthpatchUpdated(out, version, release.version);
    return true;
}

bool StepUpgrade_Run(APP_CONTEXT *const run, const char *const version)
{
    if (run->exe_dir == nullptr) {
        return true;
    }
    M_UPDATE update = {
        .run = run,
        .platform = Install_GetPlatform(),
    };
    update.othpatch_is_release =
        Release_ParseVersion(version, &update.othpatch_version);
    bool ok = M_UpdateOthpatch(&update, version);
    for (int32_t i = 0; i < APP_GAME_COUNT; i++) {
        const char *const path = Install_GetGamePath(
            update.platform, run->exe_dir, g_Games[i], run->arena);
        if (File_Exists(path) && !M_UpdateGame(&update, g_Games[i], path)) {
            ok = false;
        }
    }
    return ok;
}
