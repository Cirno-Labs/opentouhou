#pragma once

#include <stdint.h>

#define SPELL_CARD_COUNT 0x8d
#define SPELL_CARD_NAME_BYTES 0x31
#define SCORE_SHOT_COUNT 6
#define SCORE_DIFFICULTY_COUNT 6
#define SCORE_STAGE_COUNT 6
#define SCORE_PRACTICE_DIFFICULTY_COUNT 4
// The shot_index slot of the counts by shot that holds every shot's total.
#define SCORE_ALL_SHOTS 6
#define PLAY_STATS_DIFFICULTY_COUNT 7 // the difficulties, then the totals

// Holds a score-file chunk header. Serialization reads and writes each
// field separately.
typedef struct {
    uint32_t magic;
    uint16_t size;
    uint16_t size2;
    uint8_t version;
    uint8_t reserved[3];
} SCORE_RECORD_HEADER;

// Holds the record of one spell card, by shot_index and then for every
// shot: the best bonus, the attempts and the captures. checksum is the low
// byte of the sum of the name bytes and the counts.
typedef struct {
    SCORE_RECORD_HEADER header;
    uint32_t max_bonus[7];
    uint16_t index;
    uint8_t checksum;
    // Holds the CP932 name and its NUL, followed by what the bytes held
    // before.
    uint8_t name[SPELL_CARD_NAME_BYTES];
    uint16_t attempts[7];
    uint16_t captures[7];
} SPELL_CARD_RECORD;

// Holds the stages cleared by one shot, by difficulty, 99 for the whole
// game: without continues and with.
typedef struct {
    SCORE_RECORD_HEADER header;
    uint8_t no_continues[6];
    uint8_t with_continues[6];
    uint8_t shot;
    uint8_t reserved[3];
} CLEAR_RECORD;

// Holds the practice score of one shot, stage and difficulty.
typedef struct {
    SCORE_RECORD_HEADER header;
    uint32_t play_count;
    uint32_t score; // in units of ten points
    uint8_t shot;
    uint8_t difficulty;
    uint8_t stage;
    uint8_t reserved;
} PRACTICE_RECORD;

// Holds the play counts of one difficulty, or their totals.
typedef struct {
    int32_t plays[7]; // with every shot, then by shot_index + 1
    int32_t retries;
    int32_t clears;
    int32_t continues;
    int32_t practices;
} PLAY_COUNTS;

// Holds the play statistics: how long the game has run, how long it has
// been played, and the play counts by difficulty.
typedef struct {
    SCORE_RECORD_HEADER header;
    uint32_t run_hours;
    uint32_t run_minutes;
    uint32_t run_seconds;
    uint32_t run_milliseconds;
    uint32_t play_hours;
    uint32_t play_minutes;
    uint32_t play_seconds;
    uint32_t play_milliseconds;
    PLAY_COUNTS counts[PLAY_STATS_DIFFICULTY_COUNT];
} PLAY_STATS_RECORD;

// Holds the name entered last, eight CP932 bytes and a NUL in twelve.
typedef struct {
    SCORE_RECORD_HEADER header;
    uint8_t name[12];
} LAST_NAME_RECORD;

// Holds one entry of a high score list. name holds eight CP932 bytes and a
// NUL; stage is 99 for a cleared game.
typedef struct {
    // Is HIGH_SCORE_MAGIC for a saved score, or HIGH_SCORE_DEFAULT_MAGIC
    // for the defaults that fill a list and that are never saved.
    uint32_t magic;
    // Marks the entry that the player is naming.
    bool is_new;
    uint8_t reserved[2];
    uint32_t score; // in units of ten points
    float slowdown; // percent
    uint8_t shot;
    uint8_t difficulty;
    uint8_t stage;
    uint8_t name[9];
    char date[6]; // MM/DD
    uint8_t continues_used;
    uint8_t reserved_end;
} HIGH_SCORE;
