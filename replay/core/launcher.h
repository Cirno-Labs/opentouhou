#pragma once

#include "core/io/writer.h"

#include <wchar.h>

// Tells one game's capture launcher from another's.
typedef struct {
    // L"th07"; names the DLL, the work directory and the default --out.
    const wchar_t *game_id;
    const char *exe_name;
    const char *exe_sha1;
    // "1.00b".
    const char *version;
    const char *cfg_name;
    // Names the work directory's copy of the replay.
    const wchar_t *replay_copy;
    // Writes a config with a window, no frame skip and no sound.
    void (*write_cfg)(WRITER *writer);
} LAUNCHER_GAME;

// Parses the capture command line (one core/args schema with its rules),
// checks the game, prepares the work directory and runs the game with its
// DLL. Returns the exit code.
int Launcher_Main(const LAUNCHER_GAME *game, int argc, char **argv);
