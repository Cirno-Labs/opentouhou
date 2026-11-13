#pragma once

#include "core/base/arena.h"
#include "core/base/result.h"
#include "core/base/types.h"
#include "core/base/utils.h"
#include "core/json/types.h"
#include "core/patch/fetch.h"
#include "core/patch/manager.h"

#include <stdio.h>

// Holds what the steps of one run of othpatch share. main.c parses the
// arguments and runs the steps of a command (step.h), context.c opens the
// patch manager, and console.c prints.

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
    // place, and whether it takes ANSI escape sequences.
    bool terminal;
    bool ansi;
    int32_t sections;
    // Holds the games whose files to download, and the patch manager for
    // them, which App_OpenPatches opens.
    const char *games[APP_GAME_COUNT];
    int32_t game_count;
    PATCH_MANAGER *manager;
    // Holds the version of the running othpatch.
    const char *version;
    // Tells whether the patches step set the language of the system.
    bool first_run;
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

// Prints the line that fmt formats, such as with the message of result,
// and consumes result.
PRINTF_FORMAT(3, 4)
void Console_Fail(FILE *out, RESULT result, const char *fmt, ...);

// Starts a section: a blank line after the section before, then heading.
// Under the heading of the first section, it prints the patches folder.
PRINTF_FORMAT(2, 3)
void Console_StartSection(APP_CONTEXT *run, const char *fmt, ...);

// Returns the number of code points of UTF-8 text.
int32_t Console_CountCodePoints(const char *text);

#define CONSOLE_FILE_LINES 4

// Holds one file line of a CONSOLE_PATCH: the file, or nullptr for a free
// line, and received of size bytes, where size is 0 while it is unknown.
typedef struct {
    const char *file;
    size_t received;
    size_t size;
} CONSOLE_FILE;

// Holds the progress of the patch that downloads: its bar, and on an ANSI
// terminal the lines of the files that download below it and the width
// that the lines share.
typedef struct {
    CONSOLE_BAR bar;
    CONSOLE_FILE files[CONSOLE_FILE_LINES];
    int32_t width;
    // Holds the number of lines of the block on the terminal, and the
    // block last drawn, so that a report that changes nothing draws
    // nothing.
    int32_t lines;
    char block[4096];
} CONSOLE_PATCH;

// Shows the progress of a patch that fetches files files, with label
// padded to width code points. Without a terminal, only Console_EndPatch
// prints, the label and the files. On a terminal, the bar of the patch
// redraws in place with '\r'. On an ANSI terminal, up to CONSOLE_FILE_LINES
// lines under the bar show the files that download, a finished file's
// line goes to the next file, and Console_EndPatch collapses the block to
// the bar of the patch.
void Console_StartPatch(
    CONSOLE_PATCH *patch, APP_CONTEXT *run, const char *label, int32_t width,
    int32_t files);
void Console_ReceiveFile(
    CONSOLE_PATCH *patch, const char *file, size_t received, size_t size);
void Console_EndFile(CONSOLE_PATCH *patch, const char *file);
void Console_EndPatch(CONSOLE_PATCH *patch);

// Draws a bar of bytes on a terminal as a PATCH_FETCH_PROGRESS received,
// whose context is the bar. Console_EndBytes ends it: on a terminal, it
// draws the bar a last time and ends its line; elsewhere, it prints the
// label and the size. arrived tells whether the fetch ended with success.
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

// Opens the patch manager for the games whose files to download: the games
// beside othpatch and the games that an installed patch has a folder for.
RESULT App_OpenPatches(APP_CONTEXT *run);
