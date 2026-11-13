#include "othpatch/context.h"
#include "othpatch/step.h"

#include <string.h>

// Holds the state of a removal: the patch that it removes, and how many
// other patches it removed.
typedef struct {
    APP_CONTEXT *run;
    const char *target;
    int32_t others;
} M_REMOVAL;

// Returns the index of the installed patch name, -1 when no installed
// patch is name, or -2 when a bare name fits several.
static int32_t M_FindInstalled(
    const PATCH_MANAGER *const manager, const char *const name)
{
    int32_t found = -1;
    for (int32_t i = 0; i < PatchManager_GetCount(manager); i++) {
        const char *const id = PatchManager_Get(manager, i)->id;
        if (strcmp(id, name) == 0) {
            return i;
        }
        const char *const slash = strchr(id, '/');
        if (strchr(name, '/') == nullptr && slash != nullptr
            && strcmp(slash + 1, name) == 0) {
            found = found == -1 ? i : -2;
        }
    }
    return found;
}

static void M_PrintAmbiguous(
    const PATCH_MANAGER *const manager, const char *const name, FILE *const out)
{
    const int32_t count = PatchManager_GetCount(manager);
    int32_t matches = 0;
    for (int32_t i = 0; i < count; i++) {
        const char *const slash = strchr(PatchManager_Get(manager, i)->id, '/');
        matches += slash != nullptr && strcmp(slash + 1, name) == 0;
    }
    fprintf(out, "%d installed patches are %s:", matches, name);
    for (int32_t i = 0, listed = 0; i < count; i++) {
        const char *const id = PatchManager_Get(manager, i)->id;
        const char *const slash = strchr(id, '/');
        if (slash != nullptr && strcmp(slash + 1, name) == 0) {
            fprintf(out, "%s %s", listed++ > 0 ? "," : "", id);
        }
    }
    fputs(". Name one as <repo>/<patch>.\n", out);
}

static void M_ReportRemoved(void *const user, const char *const id)
{
    M_REMOVAL *const removal = user;
    if (removal->run->args->verbose) {
        fprintf(removal->run->out, "%s: removed\n", id);
    }
    removal->others += strcmp(id, removal->target) != 0;
}

static bool M_RemoveFound(APP_CONTEXT *const run, const int32_t index)
{
    FILE *const out = run->out;
    const PATCH_MANAGER *const manager = run->manager;
    M_REMOVAL removal = {
        .run = run,
        .target = Console_Format(
            run->arena, "%s", PatchManager_Get(manager, index)->id),
    };
    for (int32_t i = 0; i < PatchManager_GetCount(manager); i++) {
        const PATCH_ENTRY *const entry = PatchManager_Get(manager, i);
        for (int32_t j = 0; j < entry->dependency_count; j++) {
            if (entry->dependencies[j] == index) {
                fprintf(
                    out, "%s needs %s. Remove %s first.\n", entry->id,
                    removal.target, entry->id);
                return false;
            }
        }
    }
    const PATCH_EVENTS events = {
        .user = &removal,
        .patch_removed = M_ReportRemoved,
    };
    const RESULT result =
        PatchManager_Remove(run->manager, removal.target, &events);
    if (!IS_OK(result)) {
        fprintf(out, "Could not remove %s.\n", result.message);
        Result_Ignore(result);
        return false;
    }
    if (removal.others == 0) {
        fprintf(out, "Removed %s.\n", removal.target);
    } else {
        fprintf(
            out, "Removed %s and %d %s that only it needed.\n", removal.target,
            removal.others, removal.others == 1 ? "patch" : "patches");
    }
    return true;
}

static bool M_Remove(APP_CONTEXT *const run)
{
    FILE *const out = run->out;
    const char *const name = run->args->remove;
    const RESULT opened = App_OpenPatches(run);
    const int32_t index =
        IS_OK(opened) ? M_FindInstalled(run->manager, name) : -1;
    Console_StartSection(
        run, "Removing %s...",
        index >= 0 ? PatchManager_Get(run->manager, index)->id : name);
    if (!IS_OK(opened)) {
        Console_Fail(
            out, opened, "Could not read the patches folder: %s.",
            opened.message);
        return false;
    }
    if (index == -1) {
        fprintf(
            out,
            "There is no installed patch \"%s\".\n"
            "othpatch --list shows the installed mods.\n",
            name);
        return false;
    }
    if (index == -2) {
        M_PrintAmbiguous(run->manager, name, out);
        return false;
    }
    return M_RemoveFound(run, index);
}

const APP_STEP g_StepRemove = { .run = M_Remove };
