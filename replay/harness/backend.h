#pragma once

#include "compare.h"
#include "script.h"

#include "core/args.h"
#include "core/base/arena.h"
#include "core/base/array.h"
#include "core/image/pixels.h"
#include "core/vfs/types.h"

// Defines what every backend shares: the command line of its program,
// the game's port and the driver. A backend is the platform glue that
// runs a game and calls the driver at fixed points of each frame, such
// as native.h, which runs a game built from this repository.
//
// A frame is one presentation of the game's outer loop; frame 0 is the
// first. Input applies before a frame's calcs, and the screenshot
// follows its presentation. A calc that restarts inside a frame keeps
// the frame's input and writes a row of its own.

#define RUN_MAX_FILES ARG_LIST_MAX

typedef enum {
    RUN_CAPTURE,
    RUN_REPLAY,
} RUN_ROLE;

// Selects each listed frame, and every multiple of every when it is not 0.
typedef struct {
    ARRAY(uint32_t) frames;
    uint32_t every;
} SHOTS;

// Gives a file on disk that a capture stages at path among the game's
// files, in the normal form of core/vfs.
typedef struct {
    const char *file;
    const char *path;
} RUN_FILE;

// Holds the backend program's command line:
//
//     PROGRAM capture CASE --data DIR --out DIR (--script FILE | --live)
//         [--file FILE=PATH]... [--note TEXT] [--headless]
//     PROGRAM replay CASE --data DIR --out DIR [--shots FRAMES]
//         [--against DIR] [--headless | --fast]
//
// FRAMES is a list such as 21345 or 0,100,every:300, or @FILE, a file of
// such items separated by commas or white space, with '#' comments.
typedef struct {
    RUN_ROLE role;
    // Gives the case file that a capture writes and a replay reads.
    const char *case_path;
    // Gives the directory that the game loads from.
    const char *game_dir;
    // Receives state.bin, the screenshots and report.ini of a replay, and
    // in work/ the files that the game writes.
    const char *out_dir;
    // Gives a capture's input script, which a live capture has none of.
    const char *script;
    // Lists a capture's files, which become its case's staging steps.
    RUN_FILE files[RUN_MAX_FILES];
    size_t file_count;
    // Gives a capture's note for the manifest.
    const char *note;
    // Gives the screenshots to compare with, or nullptr.
    const char *against_dir;
    SHOTS shots;
    // Draws nothing and takes no screenshots.
    bool headless;
    // Draws only the game's draw_frames frames before each screenshot.
    bool fast;
    // Makes a capture record the buttons that a person holds in place of
    // playing a script, until the game quits. A live run is not headless.
    bool live;
} RUN;

// Gives a file that a run serves at path among the game's files from
// frame 0 on.
typedef struct {
    const char *path;
    const uint8_t *data;
    size_t size;
} STAGED_FILE;

// Holds what one game gives every backend. Each function takes context
// and reads or sets the running game's memory through it.
typedef struct {
    const HARNESS_GAME *game;
    void *context;
    // Writes one value per column, then per choice, of the game after a
    // completed calc.
    void (*sample)(void *context, uint32_t *row);
    // Returns whether the game rests at its main menu with no run going.
    bool (*idle)(void *context);
    // Sets the gameplay RNG to the game's fixed state before the first
    // calc.
    void (*seed)(void *context);
    // Requests a full repaint: the game draws its whole screen again over
    // its next frames, as after a scene change. nullptr when the game
    // always does.
    void (*request_full_repaint)(void *context);
    // Lists the files that every run serves beneath its case's, such as a
    // configuration with windowed play and no sound.
    const STAGED_FILE *files;
    size_t file_count;
} PORT;

typedef struct DRIVER DRIVER;

// Reads the command line of argc and argv, with program for messages.
// Strings come from argv or arena. Sets *help and prints the help to
// stdout when argv asks for it.
RESULT Run_Parse(
    const char *program, int argc, char *const argv[], ARENA *arena, RUN *out,
    bool *help);

// Reads the case of run, or for a capture its script and files, and opens
// its outputs. backend names the backend that runs, such as "native", for
// the manifest. Rejects a case of another game, game version or columns
// version.
RESULT Driver_Open(
    const PORT *port, const RUN *run, const char *backend, DRIVER **out);

// Returns the files that the run serves and sets *count: the port's, then
// those of the staging steps in step order. Of two files of one path, the
// later one counts.
const STAGED_FILE *Driver_Staged(const DRIVER *driver, size_t *count);

// Mounts the files of Driver_Staged over vfs, read-only, above every
// source that a game mounts.
void Driver_MountStaged(const DRIVER *driver, VFS *vfs);

// Makes work/ of out_dir and mounts it over vfs, writable, above the
// staged files, so that it takes every write.
RESULT Driver_MountWork(DRIVER *driver, VFS *vfs);

// Returns the start values of the case, or of a capture's script, and sets
// *count. The port applies them as the game starts its first run.
const START_VALUE *Driver_Starts(const DRIVER *driver, size_t *count);

// Returns the buttons of the current frame: the script's, or in a live
// capture held, the buttons that a person holds.
uint32_t Driver_Buttons(DRIVER *driver, uint32_t held);

// Returns the frame rate to show, which the backend measures on the frame
// clock. A capture records measured and returns it; a replay returns the
// recorded rate.
FPS Driver_Fps(DRIVER *driver, FPS measured);

// Samples, writes and compares the game's row after a completed calc.
void Driver_EndCalc(DRIVER *driver);

// Returns whether the current frame has a screenshot.
bool Driver_WantsShot(const DRIVER *driver);

// Returns whether the current frame draws: never in a headless run or after
// a replay's last screenshot, only within the game's draw_frames before a
// screenshot in a fast run, and always else.
bool Driver_WantsDraw(const DRIVER *driver);

// Ends the current frame after its presentation. screen holds the
// logical screen when Driver_WantsShot is true, else nullptr. In a fast
// run, requests a full repaint of the port when the next frame starts the
// draw_frames before a screenshot.
RESULT Driver_EndFrame(DRIVER *driver, const IMAGE *screen);

// Returns whether the run is over. A replay ends after the frames of
// case.ini. A capture ends after the script's frames or, when it sets
// none, on the first idle frame after its last input. A live capture ends
// only when the game quits.
bool Driver_Done(const DRIVER *driver);

// Destroys driver. After a completed run, compares the screenshots with
// against_dir and writes the outputs and report.ini; after a failed one,
// writes nothing. Sets *differs when a row, a choice or a screenshot
// differs; a difference is not a failure.
RESULT Driver_Close(DRIVER *driver, bool completed, bool *differs);
