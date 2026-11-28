#pragma once

// titles.c's own state, exposed for tests; nothing outside the stage/
// module sees this.

#include "th02/stage/titles.h"

// MAIN.EXE's gaiji/title tables (DGROUP), baked in verbatim.
typedef struct {
    const STAGE_TITLE_ASSETS *assets;
    uint8_t stage;
    uint8_t music_id;
    uint8_t music_timer;
    bool demo;
    bool banner_active;
} STAGE_TITLES;

// A writable view of titles.c's private state, for tests.
extern STAGE_TITLES *const p_Titles;
