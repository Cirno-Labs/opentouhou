#pragma once

// g_Player, g_Bomb, g_Shots and g_Miss are read-only outside their owning
// modules; this fixture pulls in each module's private storage so tests
// can poke any field directly for setup and assertions.
#include "../../src/th02/player/logic.c"
#include "../../src/th02/player/render.c"
#include "../../src/th02/bomb/logic.c"
#include "../../src/th02/shots/pool.c"
#include "../../src/th02/shots/fire.c"
#include "../../src/th02/miss.c"
