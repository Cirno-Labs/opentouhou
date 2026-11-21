#pragma once

#include "backend.h"
#include "hook.h"

#include <stddef.h>
#include <stdint.h>

// Runs a PC-98 DOS game under the driver: GAMEdosbox starts the patched
// DOSBox-X with GAMEdosbox.so, which hooks the game in emulated memory.
// Both run inside the opentouhou-dosbox-x image (replay/dosbox/Dockerfile).
//
// The backend owns the VSync. A frame is one tick of a virtual clock at
// the game's frame rate, and the port's advance tells which frames have a
// VSync. The game gets a VSync only while it waits at a site: a frame ends
// there, and frames follow up to one with a VSync, whose row the backend
// writes before it raises the VSync. Work between sites takes no frames.
// At a site, the CPU halts until emulated time reaches the end of the
// frame, so emulated time follows the virtual clock; when work took longer
// than the frame, the clock starts again from the emulated time.
//
// Each frame starts with the driver's buttons: the BIOS key bitmap at
// 0000:052A holds the keys of the held buttons, and each newly pressed key
// also goes into the BIOS key buffer. The clock starts at the first exec of
// a program of the port, which is frame 0.
//
// The run mounts base/ of out_dir read-only as drive C with work/ of
// out_dir over it, and the staged files in work/. base/ links to each file
// of game_dir, under its UTF-8 name when its name is CP932, since DOSBox-X
// looks a DOS name up by its UTF-8 form. At the end, base/ is gone and
// work/ keeps only the files that the game wrote.
//
// The watchdog fails the run with exit status 2 and a histogram of the
// CS:IPs that ran, sampled every DOSBOX_WATCHDOG_TICK, when DOSBOX_WATCHDOG
// passes without a site. Both are in emulated microseconds.

#define DOSBOX_WATCHDOG 1000000
#define DOSBOX_WATCHDOG_TICK 8333

// Names a file in game_dir, upper-case 8.3, and gives its SHA-1, which the
// backend checks.
typedef struct {
    const char *name;
    const char *sha1;
} DOSBOX_FILE;

typedef struct {
    const DOSBOX_FILE *files;
    size_t file_count;
    // Lists the commands that run on drive C, such as MDRV98 -M7, after
    // which the emulator exits.
    const char *const *commands;
    size_t command_count;
    // Names the library next to the backend program. dosbox-x.conf lies
    // there too.
    const char *library;
} DOSBOX_PROGRAM;

// Starts the emulator for the command line of argv and returns the exit
// status: 0 when the run matches, 1 when it differs, 2 on an error.
// Rejects --live.
int Dosbox_Launch(const DOSBOX_PROGRAM *program, int argc, char **argv);

typedef enum {
    // The conditional jump that closes a VSync wait loop. When it is
    // taken, ends frames up to one with a VSync.
    DOSBOX_SITE_VSYNC,
    // The conditional jump of a loop that polls the GDC status until the
    // vblank ends. When it is taken, clears the vblank bit.
    DOSBOX_SITE_VBLANK_END,
    // The conditional jump of a loop that polls the input with no wait.
    // When it is taken, ends one frame.
    DOSBOX_SITE_POLL,
    // The entry of a timed delay. Halts for the port's delay_time of
    // emulated time, ending each frame whose end it passes, then returns
    // from the call. A delay that follows another with no wait between them
    // counts from the end of that one, so the work between them takes no
    // time.
    DOSBOX_SITE_DELAY,
    // Calls the port's on_site and goes on.
    DOSBOX_SITE_CALL,
} DOSBOX_SITE_KIND;

// Gives a site at segment:offset of a program's load image, relative to
// its load segment, PSP + 10h. The jump of a site is a Jcc, JMP, JCXZ or
// LOOP.
typedef struct {
    // Indexes the port's programs.
    uint8_t program;
    DOSBOX_SITE_KIND kind;
    uint16_t segment;
    uint16_t offset;
    // Tells whether a delay returns far.
    bool far;
    // Is given to the port's callbacks.
    uint8_t id;
} DOSBOX_SITE;

// Holds the PC-98 key code that a button presses and the character that
// goes with it into the key buffer.
typedef struct {
    uint32_t mask;
    uint8_t code;
    uint8_t ascii;
} DOSBOX_KEY;

typedef struct {
    PORT port;
    // Names the programs whose sites the port gives, upper-case 8.3.
    const char *const *programs;
    size_t program_count;
    const DOSBOX_SITE *sites;
    size_t site_count;
    const DOSBOX_KEY *keys;
    size_t key_count;
    // Lists paths in game_dir, with backslashes, that a run hides, such as
    // saves, so that it starts without the player's.
    const char *const *private_paths;
    size_t private_count;
    // Moves the virtual clock on by one frame and returns whether the
    // frame has a VSync.
    bool (*advance)(void *context);
    // Returns the emulated microseconds that the delay of site id waits,
    // from its arguments.
    uint64_t (*delay_time)(void *context, uint8_t id);
    // Runs at a site of DOSBOX_SITE_CALL, or nullptr.
    void (*on_site)(void *context, uint8_t id);
} DOSBOX_PORT;

// Sets the run up for port and returns the guest for the emulator. Called
// from the library's Dosbox_Attach with its arguments, the backend
// program's command line; returns nullptr on an error.
const DOSBOX_GUEST *Dosbox_Start(
    const DOSBOX_PORT *port, const DOSBOX_HOST *host, int argc, char **argv);

// Returns the index of the running program, or -1 when none of the port's
// runs.
int32_t Dosbox_Program(void);

// Returns the load segment of the running program, its PSP + 10h.
uint16_t Dosbox_LoadSegment(void);

// Returns the index in the port's sites of the last DOSBOX_SITE_VSYNC that
// ended a frame, or -1.
int32_t Dosbox_LastWait(void);

// Returns the buttons that the current frame holds.
uint32_t Dosbox_Buttons(void);

// Copies guest memory at segment:offset into out.
void Dosbox_Read(uint16_t segment, uint16_t offset, void *out, size_t size);

// Returns the current registers.
DOSBOX_REGS Dosbox_Regs(void);
