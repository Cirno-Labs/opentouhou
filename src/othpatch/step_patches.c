#include "othpatch/context.h"
#include "othpatch/step.h"

#include "othpatch/game.h"

#include "shell/folders.h"
#include "core/base/string.h"
#include "core/base/utils.h"
#include "core/ini.h"
#include "core/io/locale.h"
#include "core/io/path.h"
#include "core/json/get.h"
#include "core/patch/language.h"

#include <ctype.h>
#include <string.h>

#define M_FALLBACK_LANGUAGE "lang_en"

typedef enum {
    M_KIND_SETUP,
    M_KIND_UPDATE,
    M_KIND_INSTALL,
} M_KIND;

// Holds the state of the language section.
typedef struct {
    APP_CONTEXT *run;
    M_KIND kind;
    // Holds the patch that the heading of M_KIND_INSTALL names.
    const char *target;
    bool headed;
    // Holds the patches of the first repository.
    const PATCH_LISTED *listed;
    int32_t listed_count;
} M_LANGUAGE;

// Holds the progress of the patches that an install or an update
// downloads: the section that it belongs to, the bar of the patch that
// downloads, whether a patch was queued, the widest label of the patches,
// whether a file changed, and the patch that failed with the reason.
typedef struct {
    APP_CONTEXT *run;
    M_LANGUAGE *language;
    CONSOLE_PATCH patch;
    bool queued;
    int32_t width;
    bool changed;
    const char *failed;
    const char *reason;
} M_PROGRESS;

static const char m_Unchanged[] =
    "Your files are unchanged. Run othpatch again later.\n";

const char *StepPatches_GetPackCode(const PATCH_LISTED *const pack)
{
    return strchr(pack->id, '/') + 1 + strlen(APP_LANGUAGE_PREFIX);
}

static bool M_IsPack(const PATCH_LISTED *const patch)
{
    return Str_StartsWith(strchr(patch->id, '/') + 1, APP_LANGUAGE_PREFIX);
}

int32_t StepPatches_SortPacks(
    const PATCH_LISTED *const listed, const int32_t count, ARENA *const arena,
    const PATCH_LISTED ***const out)
{
    const PATCH_LISTED **const packs =
        Arena_AllocArr(arena, (size_t)count + 1, sizeof(*packs));
    int32_t pack_count = 0;
    for (int32_t i = 0; i < count; i++) {
        if (M_IsPack(&listed[i])) {
            packs[pack_count++] = &listed[i];
        }
    }
    *out = packs;
    return pack_count;
}

// Returns the patch id of listed, or nullptr when listed has none.
static const PATCH_LISTED *M_FindListed(
    const PATCH_LISTED *const listed, const int32_t count, const char *const id)
{
    for (int32_t i = 0; i < count; i++) {
        if (strcmp(listed[i].id, id) == 0) {
            return &listed[i];
        }
    }
    return nullptr;
}

static const PATCH_LISTED *M_FindLanguagePack(
    const PATCH_LISTED *const listed, const int32_t count,
    const char *const tag)
{
    for (int32_t i = 0; i < count; i++) {
        if (M_IsPack(&listed[i])
            && Patch_LanguageIs(StepPatches_GetPackCode(&listed[i]), tag)) {
            return &listed[i];
        }
    }
    return nullptr;
}

const PATCH_LISTED *StepPatches_FindLanguageOfCode(
    const PATCH_LISTED *const listed, const int32_t count,
    const char *const code)
{
    char tags[PATCH_LANGUAGE_MAX_TAGS][PATCH_LANGUAGE_TAG_SIZE];
    const int32_t tag_count = Patch_GetLanguageTags(code, tags);
    for (int32_t i = 0; i < tag_count; i++) {
        const PATCH_LISTED *const pack =
            M_FindLanguagePack(listed, count, tags[i]);
        if (pack != nullptr) {
            return pack;
        }
    }
    return nullptr;
}

const char *StepPatches_GetPackTitle(const PATCH_LISTED *const pack)
{
    return pack->title[0] != '\0' ? pack->title : StepPatches_GetPackCode(pack);
}

bool StepPatches_IsOriginal(const char *const name)
{
    char tags[PATCH_LANGUAGE_MAX_TAGS][PATCH_LANGUAGE_TAG_SIZE];
    return Patch_GetLanguageTags(name, tags) > 0
        && Patch_IsOriginalLanguage(tags[0]);
}

const char *StepPatches_ListProducts(const APP_CONTEXT *const run)
{
    const char *const *const names = run->game_count > 0 ? run->games : g_Games;
    const int32_t count =
        run->game_count > 0 ? run->game_count : APP_GAME_COUNT;
    const char *text = "";
    for (int32_t i = 0; i < count; i++) {
        char product[GAME_NAME_SIZE];
        Game_GetProductName(names[i], product);
        text = Console_Format(
            run->arena, "%s%s%s", text, i > 0 ? ", " : "", product);
    }
    return text;
}

// Returns the config.ini of game beside othpatch, started without
// --user-dir. The path comes from arena.
static RESULT M_FindConfig(
    const char *const exe_dir, const char *const game, ARENA *const arena,
    const char **const out)
{
    *out = nullptr;
    const FOLDERS_INPUT input = { .exe_dir = exe_dir };
    FOLDERS folders;
    const RESULT result = Folders_Find(&input, game, &folders);
    if (IS_OK(result)) {
        *out =
            Path_Join(Arena_Allocator(arena), folders.user_dir, "config.ini");
    }
    Folders_Free(&folders);
    return result;
}

static RESULT M_WriteLanguage(const char *const path, const char *const code)
{
    INI *ini;
    MUST(Ini_Load(path, &g_HeapAllocator, &ini));
    Ini_Set(ini, "patches", "language", code);
    const RESULT result = Ini_Save(ini, path);
    Ini_Free(ini);
    return result;
}

const char *StepPatches_ReadLanguage(const APP_CONTEXT *const run)
{
    for (int32_t i = 0; i < run->game_count; i++) {
        const char *path;
        INI *ini = nullptr;
        if (!Result_Ignore(
                M_FindConfig(run->exe_dir, run->games[i], run->arena, &path))
            || !Result_Ignore(Ini_Load(path, &g_HeapAllocator, &ini))) {
            continue;
        }
        const char *const code = Ini_Get(ini, "patches", "language");
        const char *const copy = code != nullptr && code[0] != '\0'
            ? Console_Format(run->arena, "%s", code)
            : nullptr;
        Ini_Free(ini);
        if (copy != nullptr) {
            return copy;
        }
    }
    return nullptr;
}

// Returns the codes of the language packs of listed, separated by ", ".
// The string comes from arena.
static const char *M_ListCodes(
    const PATCH_LISTED *const listed, const int32_t count, ARENA *const arena)
{
    const PATCH_LISTED **packs;
    const int32_t pack_count =
        StepPatches_SortPacks(listed, count, arena, &packs);
    const char *text = "";
    for (int32_t i = 0; i < pack_count; i++) {
        text = Console_Format(
            arena, "%s%s, ", text, StepPatches_GetPackCode(packs[i]));
    }
    return text;
}

static const char *M_GetLabel(
    const PATCH_ENTRY *const patch, ARENA *const arena)
{
    if (strcmp(patch->title, patch->id) == 0) {
        return patch->id;
    }
    return Console_Format(arena, "%s (%s)", patch->title, patch->id);
}

// Prints the heading of the language section on the first call, and with
// --verbose the folders that it uses.
static void M_Head(M_LANGUAGE *const language)
{
    if (language->headed) {
        return;
    }
    language->headed = true;
    APP_CONTEXT *const run = language->run;
    if (language->kind == M_KIND_INSTALL) {
        Console_StartSection(run, "Installing %s...", language->target);
    } else if (language->kind == M_KIND_UPDATE) {
        Console_StartSection(run, "Updating the language...");
    } else {
        Console_StartSection(run, "Setting up the language...");
    }
    if (!run->args->verbose) {
        return;
    }
    for (int32_t i = 0; i < run->game_count; i++) {
        const char *path;
        if (Result_Ignore(
                M_FindConfig(run->exe_dir, run->games[i], run->arena, &path))) {
            char product[GAME_NAME_SIZE];
            Game_GetProductName(run->games[i], product);
            fprintf(run->out, "config.ini of %s: %s\n", product, path);
        }
    }
}

static void M_PrintLanguages(const M_LANGUAGE *const language)
{
    APP_CONTEXT *const run = language->run;
    fprintf(
        run->out, "Languages for %s: %sja (original)\n",
        StepPatches_ListProducts(run),
        M_ListCodes(language->listed, language->listed_count, run->arena));
}

static void M_PrintNoLanguage(
    const M_LANGUAGE *const language, const char *const code)
{
    fprintf(language->run->out, "There is no language \"%s\".\n", code);
    M_PrintLanguages(language);
}

static bool M_OpenNetwork(M_LANGUAGE *const language)
{
    APP_CONTEXT *const run = language->run;
    const RESULT opened = PatchManager_ListPatches(
        run->manager, &language->listed, &language->listed_count);
    if (IS_OK(opened)) {
        return true;
    }
    M_Head(language);
    Console_Fail(
        run->out, opened, "Could not download the patch list: %s.",
        opened.message);
    fputs(m_Unchanged, run->out);
    return false;
}

// Prints the heading of the section of progress on the first event of an
// install that names a bare patch, which names the patch by its id.
static void M_HeadPatch(
    M_PROGRESS *const progress, const PATCH_ENTRY *const patch)
{
    M_LANGUAGE *const language = progress->language;
    if (language != nullptr && !language->headed) {
        language->target = patch->id;
        M_Head(language);
    }
}

static void M_QueuePatch(void *const user, const PATCH_ENTRY *const patch)
{
    M_PROGRESS *const progress = user;
    M_HeadPatch(progress, patch);
    progress->queued = true;
    progress->width =
        MAX(progress->width,
            Console_CountCodePoints(M_GetLabel(patch, progress->run->arena)));
}

static void M_StartPatch(
    void *const user, const PATCH_ENTRY *const patch, const int32_t files)
{
    M_PROGRESS *const progress = user;
    APP_CONTEXT *const run = progress->run;
    M_HeadPatch(progress, patch);
    const char *const label = M_GetLabel(patch, run->arena);
    progress->width = MAX(progress->width, Console_CountCodePoints(label));
    Console_StartPatch(&progress->patch, run, label, progress->width, files);
}

static void M_ReceiveFile(
    void *const user, const PATCH_ENTRY *const patch, const char *const file,
    const size_t received, const size_t size)
{
    M_PROGRESS *const progress = user;
    Console_ReceiveFile(&progress->patch, file, received, size);
}

static void M_EndFile(
    void *const user, const PATCH_ENTRY *const patch, const char *const file)
{
    M_PROGRESS *const progress = user;
    Console_EndFile(&progress->patch, file);
}

static void M_EndBarOf(M_PROGRESS *const progress)
{
    if (progress->patch.bar.total > 0) {
        Console_EndPatch(&progress->patch);
        progress->patch.bar.total = 0;
    }
}

static void M_EndPatch(
    void *const user, const PATCH_ENTRY *const patch,
    const PATCH_COUNTS *const counts)
{
    M_PROGRESS *const progress = user;
    APP_CONTEXT *const run = progress->run;
    M_EndBarOf(progress);
    if (run->args->verbose && counts->downloaded == 0 && counts->removed == 0) {
        fprintf(run->out, "%s: up to date\n", patch->id);
    } else if (run->args->verbose) {
        fprintf(
            run->out, "%s: %d downloaded, %d up to date, %d removed\n",
            patch->id, counts->downloaded, counts->up_to_date, counts->removed);
    }
    progress->changed =
        progress->changed || counts->downloaded > 0 || counts->removed > 0;
}

static void M_FailPatch(
    void *const user, const PATCH_ENTRY *const patch, const char *const reason)
{
    M_PROGRESS *const progress = user;
    M_HeadPatch(progress, patch);
    M_EndBarOf(progress);
    progress->failed = patch->id;
    progress->reason = Console_Format(progress->run->arena, "%s", reason);
}

static void M_KeepPatch(
    void *const user, const char *const id, const char *const reason)
{
    const M_PROGRESS *const progress = user;
    if (progress->run->args->verbose) {
        fprintf(
            progress->run->out, "%s: left as it is, cannot update it: %s\n", id,
            reason);
    }
}

static PATCH_EVENTS M_GetEvents(M_PROGRESS *const progress)
{
    return (PATCH_EVENTS) {
        .user = progress,
        .patch_queued = M_QueuePatch,
        .patch_started = M_StartPatch,
        .file_progress = M_ReceiveFile,
        .file_done = M_EndFile,
        .patch_done = M_EndPatch,
        .patch_failed = M_FailPatch,
        .patch_kept = M_KeepPatch,
    };
}

// Prints why an install or an update failed: the patch that did not
// download, or else the message of result with a capital first letter.
// Consumes result.
static void M_PrintFailedDownload(
    const M_PROGRESS *const progress, const RESULT result)
{
    APP_CONTEXT *const run = progress->run;
    if (progress->failed != nullptr) {
        Console_Fail(
            run->out, result, "Could not download %s: %s.", progress->failed,
            progress->reason);
        fputs(m_Unchanged, run->out);
    } else {
        Console_Fail(
            run->out, result, "%c%s.",
            toupper((unsigned char)result.message[0]), result.message + 1);
    }
}

// Brings every installed patch up to date, and sets *changed to whether a
// file was downloaded or removed.
static bool M_UpdateInstalled(M_PROGRESS *const progress, bool *const changed)
{
    APP_CONTEXT *const run = progress->run;
    const PATCH_EVENTS events = M_GetEvents(progress);
    const RESULT updated = PatchManager_Update(run->manager, &events);
    if (!IS_OK(updated) && progress->failed == nullptr) {
        Console_Fail(
            run->out, updated, "Could not download the patch list: %s.",
            updated.message);
        fputs(m_Unchanged, run->out);
        return false;
    }
    if (!IS_OK(updated)) {
        M_PrintFailedDownload(progress, updated);
        return false;
    }
    *changed = progress->changed;
    return true;
}

static void M_PrintUpdated(FILE *const out, const bool changed)
{
    if (!changed) {
        fputs("Everything was already up to date.\n", out);
    }
}

// Sets [patches] language to code in the config.ini of each game.
static bool M_SaveLanguage(
    const M_LANGUAGE *const language, const char *const code)
{
    APP_CONTEXT *const run = language->run;
    bool saved = true;
    for (int32_t i = 0; i < run->game_count; i++) {
        const char *path;
        const RESULT found =
            M_FindConfig(run->exe_dir, run->games[i], run->arena, &path);
        if (!IS_OK(found)) {
            char product[GAME_NAME_SIZE];
            Game_GetProductName(run->games[i], product);
            Console_Fail(
                run->out, found, "Could not find the config.ini of %s: %s.",
                product, found.message);
            saved = false;
            continue;
        }
        const RESULT written = M_WriteLanguage(path, code);
        if (IS_OK(written)) {
            fprintf(run->out, "Saved language = %s to %s\n", code, path);
        } else {
            Console_Fail(
                run->out, written, "Could not save language = %s to %s: %s.",
                code, path, written.message);
            saved = false;
        }
    }
    return saved;
}

static bool M_HasLanguagePack(const PATCH_MANAGER *const manager)
{
    for (int32_t i = 0; i < PatchManager_GetCount(manager); i++) {
        if (Patch_IsLanguagePack(PatchManager_Get(manager, i)->id)) {
            return true;
        }
    }
    return false;
}

static bool M_SetOriginal(M_LANGUAGE *const language, const bool system)
{
    M_Head(language);
    FILE *const out = language->run->out;
    fprintf(
        out, "%s: " APP_ORIGINAL_TITLE "\n",
        system ? "System language" : "Language");
    if (!M_SaveLanguage(language, PATCH_LANGUAGE_ORIGINAL)) {
        return false;
    }
    if (M_HasLanguagePack(language->run->manager)) {
        fputs(
            "The other language packs stay installed, so switching back "
            "downloads nothing.\n",
            out);
    }
    return true;
}

static bool M_UpdateOriginal(M_LANGUAGE *const language)
{
    M_Head(language);
    APP_CONTEXT *const run = language->run;
    fputs("Language: " APP_ORIGINAL_TITLE "\n", run->out);
    M_PROGRESS progress = { .run = run };
    bool changed;
    if (!M_UpdateInstalled(&progress, &changed)) {
        return false;
    }
    M_PrintUpdated(run->out, changed);
    return true;
}

// Finds the patch that the language section installs: the language pack
// of code for a language, or the patch that the argument names. Sets
// *title to the title of a language pack, or to nullptr for another patch.
static bool M_Resolve(
    M_LANGUAGE *const language, const char *const code, const bool system,
    const char **const id, const char **const title, bool *const matched)
{
    APP_CONTEXT *const run = language->run;
    const PATCH_LISTED *const listed = language->listed;
    const int32_t count = language->listed_count;
    *title = nullptr;
    *matched = true;
    if (strchr(code, '/') != nullptr) {
        *id = code;
        if (Patch_IsLanguagePack(code)) {
            const PATCH_LISTED *const pack = M_FindListed(listed, count, code);
            *title = pack != nullptr
                ? StepPatches_GetPackTitle(pack)
                : strchr(code, '/') + 1 + strlen(APP_LANGUAGE_PREFIX);
        }
        return true;
    }
    const PATCH_LISTED *const found =
        StepPatches_FindLanguageOfCode(listed, count, code);
    if (found != nullptr) {
        *id = found->id;
        *title = StepPatches_GetPackTitle(found);
        return true;
    }
    if (system && count > 0) {
        const char *const slash = strchr(listed[0].id, '/');
        *id = Console_Format(
            run->arena, "%.*s/%s", (int)(slash - listed[0].id), listed[0].id,
            M_FALLBACK_LANGUAGE);
        const PATCH_LISTED *const fallback = M_FindListed(listed, count, *id);
        *matched = false;
        *title = fallback != nullptr
            ? StepPatches_GetPackTitle(fallback)
            : strchr(*id, '/') + 1 + strlen(APP_LANGUAGE_PREFIX);
        return true;
    }
    if (language->kind == M_KIND_UPDATE || system) {
        M_Head(language);
        M_PrintNoLanguage(language, code);
        return false;
    }
    language->kind = M_KIND_INSTALL;
    language->target = code;
    *id = code;
    return true;
}

// Installs the language pack of code, or the patch that code names, and
// updates every installed patch. system tells whether code is the language
// of the system. The heading of a bare patch name waits for the first
// event, which gives the id of the patch.
static bool M_Install(
    M_LANGUAGE *const language, const char *const code, const bool system)
{
    APP_CONTEXT *const run = language->run;
    FILE *const out = run->out;
    if (!M_OpenNetwork(language)) {
        return false;
    }
    const char *id;
    const char *title;
    bool matched;
    if (!M_Resolve(language, code, system, &id, &title, &matched)) {
        return false;
    }
    const bool bare = strchr(id, '/') == nullptr;
    if (!bare) {
        language->target = id;
        M_Head(language);
    }
    if (title != nullptr) {
        fprintf(
            out, "%s: %s\n", system && matched ? "System language" : "Language",
            title);
    }
    M_PROGRESS progress = { .run = run, .language = language };
    const PATCH_EVENTS events = M_GetEvents(&progress);
    const RESULT installed = PatchManager_Install(run->manager, id, &events);
    if (!IS_OK(installed)) {
        M_Head(language);
        M_PrintFailedDownload(&progress, installed);
        if (bare && !progress.queued) {
            M_PrintLanguages(language);
        }
        return false;
    }
    if (language->kind == M_KIND_INSTALL) {
        fprintf(out, "Installed %s.\n", language->target);
        return true;
    }
    if (language->kind == M_KIND_UPDATE) {
        M_PrintUpdated(out, progress.changed);
        return true;
    }
    return M_SaveLanguage(
        language, strchr(id, '/') + 1 + strlen(APP_LANGUAGE_PREFIX));
}

static bool M_Run(APP_CONTEXT *const run)
{
    M_LANGUAGE language = { .run = run };
    const RESULT opened = App_OpenPatches(run);
    const char *const name = run->args->name;
    const char *code = name;
    if (name == nullptr && IS_OK(opened)) {
        code = StepPatches_ReadLanguage(run);
    }
    language.target = name;
    if (name == nullptr) {
        language.kind = code != nullptr ? M_KIND_UPDATE : M_KIND_SETUP;
    } else {
        language.kind =
            strchr(name, '/') != nullptr && !Patch_IsLanguagePack(name)
            ? M_KIND_INSTALL
            : M_KIND_SETUP;
    }
    if (!IS_OK(opened)) {
        M_Head(&language);
        Console_Fail(
            run->out, opened, "Could not read the patches folder: %s.",
            opened.message);
        return false;
    }
    if (run->game_count == 0) {
        M_Head(&language);
        fputs("There is no opentouhou game beside othpatch.\n", run->out);
        return false;
    }
    if (code != nullptr && StepPatches_IsOriginal(code)) {
        return language.kind == M_KIND_UPDATE ? M_UpdateOriginal(&language)
                                              : M_SetOriginal(&language, false);
    }
    if (code != nullptr) {
        return M_Install(&language, code, false);
    }
    char system[PATCH_LANGUAGE_TAG_SIZE];
    Patch_GetLocaleCode(Locale_GetPreferred(), system);
    const bool set = system[0] != '\0' && StepPatches_IsOriginal(system)
        ? M_SetOriginal(&language, true)
        : M_Install(&language, system, true);
    run->first_run = set;
    return set;
}

const APP_STEP g_StepPatches = { .run = M_Run };
