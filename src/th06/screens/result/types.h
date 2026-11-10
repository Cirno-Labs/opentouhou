#pragma once

#include <stdint.h>

// Holds a score-file chunk header. Serialization reads and writes each
// field separately.
typedef struct {
    uint32_t magic;
    uint16_t size;
    uint16_t size2;
    uint8_t version;
    uint8_t reserved[3];
} SCORE_RECORD_HEADER;

// Holds a spell-card score record. name is UTF-8. name_fill preserves
// the random bytes used for an unnamed card in original-format saves.
typedef struct {
    SCORE_RECORD_HEADER header;
    int32_t bonus;
    uint16_t index;
    uint8_t checksum;
    uint8_t history[5];
    uint8_t name_fill[36];
    char name[109];
    uint16_t attempts;
    uint16_t captures;
} SPELL_CARD_RECORD;

// Holds the stages cleared by one shot, by difficulty. A value of 99
// means the game was cleared.
typedef struct {
    SCORE_RECORD_HEADER header;
    uint8_t without_continues[5];
    uint8_t with_continues[5];
    uint8_t shot;
    uint8_t reserved;
} CLEAR_RECORD;

// Holds the practice score of one shot, stage and difficulty.
typedef struct {
    SCORE_RECORD_HEADER header;
    uint32_t score;
    uint8_t shot;
    uint8_t difficulty;
    uint8_t stage;
    uint8_t reserved;
} PRACTICE_RECORD;
