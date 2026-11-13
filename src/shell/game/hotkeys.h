#pragma once

#include "core/input.h"

// Holds the shell's own hotkey actions, added as a schema to the game's input.
typedef struct {
    bool screenshot;
    bool fullscreen;
} HOTKEYS;

extern const INPUT_SCHEMA g_HotkeysSchema;
