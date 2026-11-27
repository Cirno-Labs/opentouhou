#pragma once

#include "replay/harness/game.h"
#include "replay/th07/fields.h"

// Describes th07 to the harness: its button word and the state columns
// of fields.h.

#define TH07_COLUMN_ENUM(name, kind) TH07_COLUMN_##name,
enum { TH07_COLUMNS(TH07_COLUMN_ENUM) TH07_COLUMN_COUNT };
#undef TH07_COLUMN_ENUM

// Gives id "th07", version "1.00b", 60 frames per second, the 640 by 480
// screen and shows_fps.
extern const HARNESS_GAME g_Th07Harness;
