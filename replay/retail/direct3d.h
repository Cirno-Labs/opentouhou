#pragma once

#include "core/base/result.h"
#include "core/image/pixels.h"

#include <windows.h>

#include <d3d8.h>

// Hooks module's Direct3DCreate8 so that every device it creates calls
// present before each Present. Returns whether module imports
// Direct3DCreate8.
bool Direct3d_Install(
    HMODULE module, void (*present)(IDirect3DDevice8 *device));

// Makes the devices draw, or drop every draw, clear and presentation until
// the next call. They draw from Direct3d_Install on.
void Direct3d_SetDrawing(bool draw);

// Reads device's back buffer as an RGBA8 image with full alpha.
RESULT Direct3d_ReadBackBuffer(IDirect3DDevice8 *device, IMAGE *out);
