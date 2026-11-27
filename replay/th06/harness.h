#pragma once

#include "replay/harness/game.h"
#include "replay/th06/fields.h"

// Describes th06 to the harness: its button word and the state columns
// of fields.h.

#define TH06_COLUMN_ENUM(name, kind) TH06_COLUMN_##name,
enum { TH06_COLUMNS(TH06_COLUMN_ENUM) TH06_COLUMN_COUNT };
#undef TH06_COLUMN_ENUM

extern const HARNESS_GAME g_Th06Harness;
