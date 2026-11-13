// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <stdint.h>

// Records the sound effects and the music changes that a tick asks for.
// The simulation writes these events and never reads them back.
// SoundPlayer_Update plays them after the tick and clears them. Without a
// player, as in headless runs, they stay recorded and change nothing.

#define SOUND_QUEUE_SIZE 3
#define SOUND_PATH_SIZE 256

// Names the sound effects by their index in the sound table. ECL scripts
// and the player's shot tables give these indices as numbers.
typedef enum {
    SOUND_PLAYER_SHOT = 0,
    SOUND_PLAYER_SHOT_QUIET = 1,
    SOUND_ENEMY_DEATH_0 = 2,
    SOUND_ENEMY_DEATH_1 = 3,
    SOUND_PLAYER_DEATH = 4,
    SOUND_POWER_0 = 5,
    SOUND_POWER_1 = 6,
    SOUND_SHOT_0 = 7,
    SOUND_SHOT_1 = 8,
    SOUND_SHOT_2 = 9,
    SOUND_OK = 10,
    SOUND_CANCEL = 11,
    SOUND_SELECT = 12,
    SOUND_GUN = 13,
    SOUND_CARD = 14,
    SOUND_SHOT_0_LOUD = 15,
    SOUND_LASER_0 = 16,
    SOUND_LASER_1 = 17,
    SOUND_ENEMY_DEATH_BIG = 18,
    SOUND_SPARK = 19,
    SOUND_DAMAGE = 20,
    SOUND_ITEM = 21,
    SOUND_SHOT_0_QUIET = 22,
    SOUND_SHOT_1_QUIET = 23,
    SOUND_SHOT_2_QUIET = 24,
    SOUND_KIRA_0 = 25,
    SOUND_KIRA_1 = 26,
    SOUND_KIRA_2 = 27,
    SOUND_EXTEND = 28,
    SOUND_TIMEOUT = 29,
    SOUND_GRAZE = 30,
    SOUND_POWER_UP = 31,
    SOUND_COUNT = 32,
} SOUND;

typedef enum {
    SOUND_MUSIC_KEEP,
    SOUND_MUSIC_PLAY,
    SOUND_MUSIC_STOP,
} SOUND_MUSIC_CHANGE;

typedef struct {
    // Holds the sound effects to play, in order, and -1 after the last.
    int32_t queue[SOUND_QUEUE_SIZE];
    SOUND_MUSIC_CHANGE music;
    // Holds the path of SOUND_MUSIC_PLAY as the data names it, such as
    // "bgm/th06_01.mid".
    char music_path[SOUND_PATH_SIZE];
    // Asks for a fade out over fade_seconds, after the music change.
    bool fade_out;
    float fade_seconds;
} SOUND_EVENTS;

extern SOUND_EVENTS g_SoundEvents;

// Empties the queue and drops the music change and the fade.
void Sound_Reset(void);

// Queues sound effect sound, unless it is queued already or the queue is
// full. An index from SOUND_COUNT to 0x7f takes a place in the queue and
// plays nothing; a negative index or a larger one is ignored.
void Sound_Queue(int32_t sound);

// Asks for the music at path, which replaces the music that plays, and
// drops a fade asked for earlier in the tick. A path that does not fit in
// SOUND_PATH_SIZE is ignored.
void Sound_PlayMusic(const char *path);
// Asks for the music to stop, and drops a fade asked for earlier in the
// tick.
void Sound_StopMusic(void);
// Asks for the music to fade out over seconds and then stop. After
// Sound_StopMusic in the same tick, does nothing.
void Sound_FadeOutMusic(float seconds);
