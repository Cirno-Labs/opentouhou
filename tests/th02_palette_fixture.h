#pragma once

// g_Palette is read-only outside th02/palette.c; this fixture pulls in
// the module's private storage so tests can poke it directly for setup
// and assertions.
#include "../src/th02/palette.c"
