#pragma once

#include <stdint.h>

typedef enum {
    REPLAY_RECORD = 0,
    REPLAY_PLAY = 1,
} REPLAY_MODE;

// Marks what happened in a calc of a recording.
typedef enum {
    REPLAY_EVENT_BOMB = 0x1,
    REPLAY_EVENT_HIT = 0x2,
    REPLAY_EVENT_DEATH = 0x4,
    REPLAY_EVENT_BORDER = 0x8,
    REPLAY_EVENT_BORDER_BROKEN = 0x10,
    REPLAY_EVENT_ENEMY_GONE = 0x20,
    REPLAY_EVENT_ITEM = 0x40,
    REPLAY_EVENT_PAUSE = 0x100, // a pause was requested
} REPLAY_EVENT;

// Holds what one calc of a recording stores. Recording stores a frame on
// every calc of a stage.
typedef struct {
    uint16_t buttons;
    // Holds the REPLAY_EVENT bits of the calc. The empty frames at the
    // start and the end of a stage hold uninitialized words.
    uint16_t events;
} REPLAY_FRAME;

// Holds the game's state at the start of a stage, then the stage's frames.
typedef struct {
    // Holds the score at the end of this stage, in units of ten points.
    // The next stage restores it.
    int32_t score;
    uint16_t rng_seed;
    int32_t point_items_total;
    uint8_t power;
    uint8_t lives;
    uint8_t bombs;
    uint8_t rank;
    uint8_t power_item_combo;
    // Hold the cherry counters without the game's obfuscation offset.
    int32_t cherry;
    int32_t cherry_max;
    int32_t cherry_plus;
    int32_t graze_total;
    uint8_t spell_cards; // the spell cards captured
    // Hold the point item extends earned (-1 once none are left) and the
    // point items the next one needs.
    int32_t point_extends;
    int32_t next_extend_points;
    // Holds every physical entry: the initial empty frame, the recorded
    // frames, one empty frame (two in the final stage).
    REPLAY_FRAME *frames;
    int32_t frame_count;
} REPLAY_STAGE;

typedef struct {
    uint8_t shot; // character * 2 + shot type
    uint8_t difficulty; // 4 is Extra, 5 Phantasm
    char date[6]; // MM/DD
    char name[37]; // UTF-8, decoded from twelve CP932 bytes
    uint32_t score; // in units of ten points
    float slowdown; // percent
    uint8_t config[0x38]; // the original's th07.cfg as recorded
    // Hold the identity of the executable that recorded the replay.
    uint32_t exe_size;
    uint32_t exe_checksum;
    char exe_version[6];
    // Hold slots 1 to 7: a slot is the stage, and Extra and Phantasm both
    // use slot 7 (Phantasm runs as game stage 8).
    REPLAY_STAGE *stages[8];
    // Hold each slot's frame rates, one byte for every 30 calcs (0x80
    // set while the clock ran fast), and one more byte.
    uint8_t *fps[8];
    int32_t fps_counts[8];
} REPLAY_FILE;

// Holds what the replay menu shows of a replay.
typedef struct {
    uint8_t shot; // character * 2 + shot type
    uint8_t difficulty;
    char date[6]; // MM/DD
    char name[37]; // UTF-8
    uint32_t score; // in units of ten points
    float slowdown;
    bool stages[8]; // whether slots 1 to 7 are present
    // Hold the score at the end of each present slot, in units of ten
    // points.
    int32_t stage_scores[8];
} REPLAY_SUMMARY;

// Holds what the result screen stores in a replay that it saves.
typedef struct {
    const char *name; // UTF-8, at most twelve CP932 bytes
    const char *date; // MM/DD
    float slowdown; // percent
} REPLAY_SAVE_INFO;
