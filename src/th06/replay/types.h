#pragma once

#include <stdint.h>

typedef enum {
    REPLAY_RECORD = 0,
    REPLAY_PLAY = 1,
} REPLAY_MODE;

typedef struct {
    int32_t frame;
    uint16_t buttons;
} REPLAY_FRAME;

typedef struct {
    // Holds the score at the end of this stage. The next stage restores it.
    int32_t score;
    uint16_t rng_seed;
    uint16_t point_items_total;
    uint8_t power;
    uint8_t lives;
    uint8_t bombs;
    uint8_t rank;
    uint8_t power_item_combo;
    REPLAY_FRAME *frames;
    int32_t frame_count;
} REPLAY_STAGE;

typedef struct {
    uint8_t shot; // character * 2 + shot type
    uint8_t difficulty;
    char date[9];
    char name[25]; // UTF-8, decoded from eight CP932 bytes
    uint32_t score;
    float slowdown;
    REPLAY_STAGE *stages[8]; // stages 1 through 7
} REPLAY_FILE;

// Holds what the replay menu shows of a replay and what starting it takes.
typedef struct {
    uint8_t shot; // character * 2 + shot type
    uint8_t difficulty;
    char date[9];
    char name[25]; // UTF-8
    uint32_t score;
    float slowdown;
    bool stages[8]; // whether stages 1 to 7 are present
    int32_t stage_scores[8]; // the score at the end of each present stage
    // Hold the lives and bombs of the first stage present.
    uint8_t lives;
    uint8_t bombs;
} REPLAY_SUMMARY;

typedef struct {
    const char *name;
    const char *date; // MM/DD/YY
    float achieved_frames;
    float expected_frames;
} REPLAY_SAVE_INFO;
