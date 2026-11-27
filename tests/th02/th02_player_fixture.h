#pragma once

// g_Player, g_Bomb, g_Shots and g_Miss are read-only outside their owning
// modules; this fixture pulls in each module's private view so tests can
// poke any field directly for setup and assertions.
#include "th02/bomb/private.h"
#include "th02/miss_private.h"
#include "th02/player/private.h"
#include "th02/shots/private.h"
