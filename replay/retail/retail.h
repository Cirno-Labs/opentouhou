#pragma once

#include "replay/harness/backend.h"
#include "replay/retail/clock.h"

#include <stddef.h>
#include <stdint.h>

// Runs a retail Windows game under the driver: GAMEretail.exe starts the
// game suspended and injects GAMEretail.dll, which hooks it.

typedef struct {
    // Names the program in game_dir and gives its SHA-1, which the backend
    // checks.
    const char *exe;
    const char *exe_sha1;
    // Names the DLL next to the backend program.
    const char *dll;
} RETAIL_PROGRAM;

typedef struct {
    PORT port;
    // Lists the code whose clock reads move on at once, so that the game's
    // timed waits end without the frame clock moving.
    const CLOCK_RANGE *pacing;
    size_t pacing_count;
    // Lists paths below game_dir, with backslashes, that only work/ gives,
    // such as saves, so that a run starts without the player's.
    const char *const *private_paths;
    size_t private_count;
    // Patches the program: its input read calls Retail_Buttons, its calc
    // Retail_StartCalc, Retail_RestartCalc and Retail_EndCalc and its rate
    // display Retail_Fps.
    RESULT (*hook)(void *context);
} RETAIL_PORT;

// Starts the program for the command line of argv and returns the exit
// status: 0 when the run matches, 1 when it differs, 2 on an error.
int Retail_Launch(const RETAIL_PROGRAM *program, int argc, char **argv);

// Sets the process up for the run and calls port's hook. Called from the
// DLL's entry; exits the process with status 2 on an error.
void Retail_Attach(const RETAIL_PORT *port);

// Closes the driver of a run that the game ended itself, and exits with
// status 2 when a run other than a live capture had not ended. Called from
// the DLL's entry when the process detaches.
void Retail_Detach(void);

// Returns the buttons that the current frame applies. A live capture calls
// read, the program's own input read, and records what a person holds.
uint32_t Retail_Buttons(uint32_t (*read)(void));

// Brackets one calc. The first sets the gameplay RNG; Retail_EndCalc
// samples the row.
void Retail_StartCalc(void);
void Retail_EndCalc(void);

// Samples the row of a calc that restarts within the frame.
void Retail_RestartCalc(void);

// Returns the frame rate that the program's display shows in place of
// measured.
FPS Retail_Fps(FPS measured);
