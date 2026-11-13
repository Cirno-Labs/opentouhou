#include "othpatch/context.h"
#include "othpatch/step.h"

#include "core/patch/language.h"

#include <string.h>

static bool M_IsInstalled(
    const PATCH_MANAGER *const manager, const char *const id)
{
    for (int32_t i = 0; i < PatchManager_GetCount(manager); i++) {
        if (strcmp(PatchManager_Get(manager, i)->id, id) == 0) {
            return true;
        }
    }
    return false;
}

static void M_PrintLanguageLine(
    FILE *const out, const char *const code, const char *const title,
    const bool in_use, const bool installed)
{
    fprintf(
        out, "  %-8s %s%s\n", code, title,
        in_use          ? " (in use)"
            : installed ? " (installed)"
                        : "");
}

static bool M_List(APP_CONTEXT *const run)
{
    FILE *const out = run->out;
    ARENA *const arena = run->arena;
    const RESULT opened = App_OpenPatches(run);
    if (!IS_OK(opened)) {
        Console_Fail(
            out, opened, "Could not read the patches folder: %s.",
            opened.message);
        return false;
    }
    const PATCH_LISTED *listed;
    int32_t listed_count;
    const RESULT fetched =
        PatchManager_ListPatches(run->manager, &listed, &listed_count);
    if (!IS_OK(fetched)) {
        Console_Fail(
            out, fetched, "Could not download the patch list: %s.",
            fetched.message);
        return false;
    }
    const char *const code = StepPatches_ReadLanguage(run);
    const bool original = code != nullptr && StepPatches_IsOriginal(code);
    const PATCH_LISTED *const in_use = code != nullptr && !original
        ? StepPatches_FindLanguageOfCode(listed, listed_count, code)
        : nullptr;
    const PATCH_LISTED **packs;
    const int32_t pack_count =
        StepPatches_SortPacks(listed, listed_count, arena, &packs);
    fprintf(out, "Languages for %s:\n", StepPatches_ListProducts(run));
    bool listed_original = false;
    for (int32_t i = 0; i < pack_count; i++) {
        const char *const pack_code = StepPatches_GetPackCode(packs[i]);
        if (!listed_original
            && strcmp(PATCH_LANGUAGE_ORIGINAL, pack_code) < 0) {
            M_PrintLanguageLine(
                out, PATCH_LANGUAGE_ORIGINAL, APP_ORIGINAL_TITLE, original,
                false);
            listed_original = true;
        }
        M_PrintLanguageLine(
            out, pack_code, StepPatches_GetPackTitle(packs[i]),
            in_use == packs[i], M_IsInstalled(run->manager, packs[i]->id));
    }
    if (!listed_original) {
        M_PrintLanguageLine(
            out, PATCH_LANGUAGE_ORIGINAL, APP_ORIGINAL_TITLE, original, false);
    }

    int32_t mods = 0;
    for (int32_t i = 0; i < PatchManager_GetCount(run->manager); i++) {
        const PATCH_ENTRY *const entry = PatchManager_Get(run->manager, i);
        if (entry->role != PATCH_ROLE_MOD) {
            continue;
        }
        if (mods++ == 0) {
            fputs("Installed mods:\n", out);
        }
        if (strcmp(entry->title, entry->id) != 0) {
            fprintf(out, "  %s (%s)\n", entry->title, entry->id);
        } else {
            fprintf(out, "  %s\n", entry->id);
        }
    }
    if (mods == 0) {
        fputs("Installed mods: none\n", out);
    }
    return true;
}

const APP_STEP g_StepList = { .run = M_List };
