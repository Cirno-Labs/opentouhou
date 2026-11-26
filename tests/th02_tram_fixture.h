#pragma once

// g_Tram is read-only outside th02/tram/cells.c; this fixture pulls in the
// module's private storage so tests can poke it directly for setup
// and assertions.
#include "../src/th02/tram/cells.c"
