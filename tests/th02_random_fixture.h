#pragma once

// g_Rng is read-only outside th02/random.c; this fixture pulls in the
// module's private storage so tests can poke it directly for setup
// and assertions.
#include "../src/th02/random.c"
