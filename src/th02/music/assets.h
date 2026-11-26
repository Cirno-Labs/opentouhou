#pragma once

// The songs: the fifteen PMD files of the game's music, loaded once and
// played by their number.

#include "pc98/pmd_data.h"

#include "core/vfs.h"

typedef enum {
    MUSIC_STAGE0,
    MUSIC_STAGE1,
    MUSIC_STAGE2,
    MUSIC_STAGE3,
    MUSIC_STAGE4,
    MUSIC_STAGE5,
    MUSIC_BOSS1,
    MUSIC_BOSS2,
    MUSIC_BOSS3,
    MUSIC_BOSS4,
    MUSIC_BOSS5,
    MUSIC_MIMA,
    MUSIC_OP,
    MUSIC_END1,
    MUSIC_ENDING,
    MUSIC_SONG_COUNT,
} MUSIC_ID;

typedef struct {
    PMD_SONG *songs[MUSIC_SONG_COUNT];
} MUSIC_ASSETS;

// Returns the PMD filename of id, or nullptr.
const char *MusicAssets_Path(MUSIC_ID id);

// Loads every song into the allocator, which keeps partial loads.
RESULT MusicAssets_Load(
    VFS *files, const ALLOCATOR *allocator, MUSIC_ASSETS *out);
