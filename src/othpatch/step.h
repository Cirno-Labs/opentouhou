#pragma once

#include "othpatch/context.h"

// A step is one part of a command: a section of a run, or --list or
// --remove. Its run returns whether the step ended well; a failed step
// has printed why.
typedef struct {
    bool (*run)(APP_CONTEXT *context);
} APP_STEP;

// Sets up, updates or installs the language or the patch that the
// arguments name, and updates every installed patch.
extern const APP_STEP g_StepPatches;
// Checks for new releases of othpatch and of each game beside it.
extern const APP_STEP g_StepUpgrade;
// Lists the languages and the installed mods.
extern const APP_STEP g_StepList;
// Removes a patch and the patches that only it needs.
extern const APP_STEP g_StepRemove;
