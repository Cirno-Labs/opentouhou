#pragma once

#include <stddef.h>
#include <stdint.h>

// Defines the calls between the patched DOSBox-X (dosbox-x.patch and
// host.cpp) and the backend library that it loads. Addresses are linear
// guest addresses, segment * 16 + offset. Emulated time is in microseconds
// since the emulator started. The header is C and C++.

#define DOSBOX_HOOK_ABI 2

// Names the library's one export, a DOSBOX_ATTACH_FN.
#define DOSBOX_ATTACH_SYMBOL "Dosbox_Attach"

typedef struct {
    uint16_t ax, bx, cx, dx, si, di, bp, sp;
    uint16_t cs, ds, es, ss, ip, flags;
} DOSBOX_REGS;

// Holds what the emulator gives the library. Calls other than mount are
// valid only within on_exec, on_exit, on_site and on_timer; mount is valid
// only within on_start.
typedef struct {
    uint32_t abi;
    // Copies guest memory at address into out, or data into it.
    void (*read)(uint32_t address, void *out, size_t size);
    void (*write)(uint32_t address, const void *data, size_t size);
    // Reads or sets the registers. Within on_site, a set takes effect before
    // the site's instruction runs; when it moves CS:IP, execution goes on at
    // the new address.
    void (*get_regs)(DOSBOX_REGS *out);
    void (*set_regs)(const DOSBOX_REGS *regs);
    // Makes the emulator call on_site before each run of the instruction at
    // address, or no longer. Needs the normal CPU core.
    void (*set_site)(uint32_t address);
    void (*clear_site)(uint32_t address);
    // Stops the emulator's own VSync: from now on it raises no IRQ 2 and
    // sets no vblank bit in either GDC's status, and INT 18h AH=30h and
    // AH=42h no longer wait for the vblank.
    void (*own_vsync)(void);
    // Sets the vblank bit of both GDCs' status, which stays set until
    // clear_vblank. Raises IRQ 2 when the guest armed it through port 64h,
    // as the emulator's own VSync does.
    void (*raise_vsync)(void);
    void (*clear_vblank)(void);
    // Returns whether a raised IRQ 2 waits for delivery.
    bool (*vsync_pending)(void);
    // Makes the CPU wait at the current site as on HLT, until an interrupt
    // or until emulated time reaches deadline. The site's instruction is
    // reached again afterwards and calls on_site again. Valid only within
    // on_site.
    void (*halt)(uint64_t deadline);
    // Returns emulated time.
    uint64_t (*now)(void);
    // Draws the shown screen, the graphics and text layers, into 640 by 400
    // pixels of XRGB8888. Analog palette levels are scaled by 17.
    void (*render)(uint32_t *pixels);
    // Mounts base as drive with overlay over it, which takes every write,
    // and hides the count names of hidden in base. Paths are the host's;
    // names are DOS paths below the drive's root. Leaves base unchanged and
    // keeps DOSBox-X's deletion markers, .DBOVERLAY files, in overlay.
    bool (*mount)(
        char drive, const char *base, const char *overlay,
        const char *const *hidden, size_t count);
    // Ends emulation once the current callback returns, and makes the
    // emulator exit with status.
    void (*quit)(int status);
    // Makes the emulator call on_timer each period of emulated time from now
    // on, between two instructions, or no longer when period is 0.
    void (*set_timer)(uint64_t period);
} DOSBOX_HOST;

// Holds what the library gives the emulator.
typedef struct {
    void *context;
    // Runs once before the first autoexec command.
    void (*on_start)(void *context);
    // Tells that program name, upper-case 8.3, has loaded at psp and runs its
    // first instruction next.
    void (*on_exec)(void *context, const char *name, uint16_t psp);
    // Tells that the program at psp ended with status, or stayed resident.
    void (*on_exit)(void *context, uint16_t psp, uint8_t status);
    // Tells that execution reached a site, before its instruction.
    void (*on_site)(void *context, uint32_t address);
    // Runs on each tick of set_timer; get_regs gives the registers of the
    // code that runs then.
    void (*on_timer)(void *context);
    // Runs once after emulation ends, when no host call is valid. Returns the
    // emulator's exit status, given the one that it would return.
    int (*on_shutdown)(void *context, int status);
} DOSBOX_GUEST;

// Attaches the library with the arguments after "--" of
// dosbox-x -harness LIBRARY [OPTIONS...] -- ARGS... Returns null on an
// error, and the emulator then exits with status 2.
typedef const DOSBOX_GUEST *(*DOSBOX_ATTACH_FN)(
    const DOSBOX_HOST *host, int argc, char **argv);
