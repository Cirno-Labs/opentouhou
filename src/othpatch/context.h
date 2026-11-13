#pragma once

#include "core/base/arena.h"
#include "core/base/result.h"
#include "core/base/types.h"
#include "core/base/utils.h"
#include "core/json/types.h"
#include "core/patch/fetch.h"
#include "core/patch/manager.h"

#include <stdio.h>

// Holds what the steps of one run of othpatch share: main.c parses the
// arguments and runs the steps in order, step_patches.c sets up the language
// and updates the patches, installed.c lists and removes the installed
// patches, step_upgrade.c installs new releases, and console.c prints.

#define APP_GAME_COUNT 1
#define APP_LANGUAGE_PREFIX "lang_"
#define APP_ORIGINAL_TITLE "original Japanese"

// Lists the games whose files othpatch can download, besides the files
// that every game shares.
extern const char *const g_Games[APP_GAME_COUNT];

typedef struct {
    const char *dir;
    const char *repo;
    const char *releases;
    const char *name;
    const char *remove;
    bool list;
    bool help;
    bool version;
    bool update;
    bool verbose;
} APP_ARGS;

// Holds what every step of one run uses.
typedef struct {
    const APP_ARGS *args;
    const char *exe_dir;
    const char *dir;
    ARENA *arena;
    FILE *in;
    FILE *out;
    // Tells whether out is a terminal, where a progress bar redraws in
    // place.
    bool terminal;
    int32_t sections;
    // Holds the games whose files to download, and the patch manager for
    // them, which Installed_Open opens.
    const char *games[APP_GAME_COUNT];
    int32_t game_count;
    PATCH_MANAGER *manager;
} APP_CONTEXT;

// Holds a progress bar: a label padded to width code points, and the
// files or the bytes that have arrived.
typedef struct {
    APP_CONTEXT *run;
    const char *label;
    int32_t width;
    int32_t done;
    int32_t total;
    size_t received;
    size_t expected;
    // Holds the line last drawn, so that a report that changes nothing
    // draws nothing.
    char line[512];
} CONSOLE_BAR;

// Returns a string that fmt formats, from arena.
PRINTF_FORMAT(2, 3)
char *Console_Format(ARENA *arena, const char *fmt, ...);

// Prints "othpatch: <message>" for a failure before the first section, and
// returns 1. Console_FailResult prints "<context>: <reason>" so and consumes
// result.
PRINTF_FORMAT(2, 3)
int Console_Fail(FILE *out, const char *fmt, ...);
int Console_FailResult(FILE *out, const char *context, RESULT result);

// Prints "<what>: <reason>." and consumes result.
void Console_PrintFailure(FILE *out, const char *what, RESULT result);

// Starts a section: a blank line after the section before, then heading.
PRINTF_FORMAT(2, 3)
void Console_StartSection(APP_CONTEXT *run, const char *fmt, ...);

// Returns the number of code points of UTF-8 text.
int32_t Console_CountCodePoints(const char *text);

// Draws a bar of files on a terminal, with done of total files.
// Console_EndFiles ends it: on a terminal, it draws the bar a last time and
// ends its line; elsewhere, it prints the label and the files.
void Console_DrawFiles(CONSOLE_BAR *bar);
void Console_EndFiles(CONSOLE_BAR *bar);

// Draws a bar of bytes on a terminal as a PATCH_FETCH_PROGRESS received,
// whose context is the bar. Console_EndBytes ends it as Console_EndFiles
// does; arrived tells whether the fetch ended with success.
void Console_ReceiveBytes(
    void *context, const PATCH_FETCH *fetch, size_t received, size_t expected);
void Console_EndBytes(CONSOLE_BAR *bar, bool arrived);

// Returns the code of the language pack pack, the name after "lang_".
const char *StepPatches_GetPackCode(const PATCH_LISTED *pack);

// Returns the language packs of the count patches of listed, in byte
// order of their names. The array comes from arena.
int32_t StepPatches_SortPacks(
    const PATCH_LISTED *listed, int32_t count, ARENA *arena,
    const PATCH_LISTED ***out);

// Returns the language pack of listed for the language code, or nullptr
// when listed has none.
const PATCH_LISTED *StepPatches_FindLanguageOfCode(
    const PATCH_LISTED *listed, int32_t count, const char *code);

// Returns the title of the language pack pack, as repo.js gives it, or the
// code of the pack when repo.js gives none.
const char *StepPatches_GetPackTitle(const PATCH_LISTED *pack);

// Returns whether the language code name is the original Japanese.
bool StepPatches_IsOriginal(const char *name);

// Returns the product names of the games of run, separated by ", ", or of
// every game that othpatch knows when there are none. The string comes
// from the arena of run.
const char *StepPatches_ListProducts(const APP_CONTEXT *run);

// Returns the [patches] language of the first game of run that sets one,
// or nullptr when none does. The string comes from the arena of run.
const char *StepPatches_ReadLanguage(const APP_CONTEXT *run);

// Runs the first section: sets up, updates or installs the language or the
// patch that the arguments name, and updates every installed patch.
// *first_run tells whether the language of the system was set.
bool StepPatches_Run(APP_CONTEXT *run, bool *first_run);

// Opens the patch manager for the games whose files to download: the games
// beside othpatch and the games that an installed patch has a folder for.
RESULT Installed_Open(APP_CONTEXT *run);

// Runs --list and --remove, and returns the exit code.
int Installed_List(APP_CONTEXT *run);
int Installed_Remove(APP_CONTEXT *run);

// Runs the section of othpatch and then of each game beside it. Returns
// whether every section ended well.
bool StepUpgrade_Run(APP_CONTEXT *run, const char *version);
