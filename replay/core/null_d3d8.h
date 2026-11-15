#pragma once

#include <windows.h>

// Replaces module's Direct3DCreate8 with one whose devices work as usual
// except that drawing, clearing and presenting do nothing. The game runs
// all its own code without rasterizing; what it copies back from the screen
// is blank. Returns whether module imports Direct3DCreate8.
bool NullD3d8_Install(HMODULE module);
