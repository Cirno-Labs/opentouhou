#pragma once

#include <stdint.h>

// Connects the places that dosbox-x.patch changes in DOSBox-X to host.cpp,
// which implements hook.h's host side. Every call does nothing without a
// library.

// Tells whether the library has set a site, so that the CPU core checks
// each instruction.
extern bool g_HarnessSites;
// Tells whether the library owns the VSync, and the vblank bit that it set.
extern bool g_HarnessOwnsVsync;
extern bool g_HarnessVblank;

// Takes -harness LIBRARY out of the command line and everything from "--"
// on, then loads the library and attaches it. Exits with status 2 on an
// error.
void Harness_Load(int *argc, char **argv);

// Runs on_site when address is a site that has not just run. Returns
// whether the CPU core must stop before the instruction, because the
// registers changed, the CPU halted or emulation ends.
bool Harness_Site(uint32_t address);

void Harness_Start(void);
void Harness_Exec(const char *path, uint16_t psp);
void Harness_Exit(uint16_t psp, uint8_t status);

// Returns the exit status of the emulator, given the one that it would
// return.
int Harness_Shutdown(int status);

// Draws the shown screen into 640 by 400 pixels; vga_draw.cpp gives it.
void Harness_Render(uint32_t *pixels);

// Copies raw graphics pages and the analog palette; vga_draw.cpp gives it.
void Harness_ReadGraphics(uint8_t *pages, uint8_t *palette);
