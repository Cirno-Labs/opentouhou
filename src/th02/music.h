#pragma once

// The music: the fifteen PMD songs, loaded once and played by their
// number, and the fade wait the game polls while a song fades out before
// it switches songs, counted in 48000-Hz samples and OPNA Timer A counts.

#include "core/base/types.h"
#include "core/vfs.h"
#include "pc98/pmd_data.h"

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

typedef struct {
    uint32_t phase;
    uint8_t timer_a;
    uint8_t volume;
    uint8_t speed;
} MUSIC_WAIT;

// Returns the PMD filename of id, or nullptr.
const char *MusicAssets_Path(MUSIC_ID id);

// Loads every song into the allocator, which keeps partial loads.
RESULT MusicAssets_Load(
    VFS *files, const ALLOCATOR *allocator, MUSIC_ASSETS *out);

// Starts the wait clock at 48000 Hz with Timer A=0; Start and Stop keep its
// phase.
void MusicWait_Init(MUSIC_WAIT *wait);
void MusicWait_Start(MUSIC_WAIT *wait);
void MusicWait_Stop(MUSIC_WAIT *wait);

// Advances by frames 48000-Hz samples, fading every eighth A count; the
// fade speed clears only past 255, not at it.
void MusicWait_Advance(MUSIC_WAIT *wait, uint32_t frames);

// Starts a fade at speed 1..127.
void MusicWait_FadeOut(MUSIC_WAIT *wait, uint8_t speed);

// Reports whether attenuation reached 255.
bool MusicWait_Ready(const MUSIC_WAIT *wait);
