#pragma once

#include <stdint.h>

#define SPELL_CARD_COUNT 64
#define SPELL_CARD_NAME_BYTES 36

// Holds a score-file chunk header. Serialization reads and writes each
// field separately.
typedef struct {
    uint32_t magic;
    uint16_t size;
    uint16_t size2;
    uint8_t version;
    uint8_t reserved[3];
} SCORE_RECORD_HEADER;

// Holds a spell-card score record. name_bytes holds the 36 bytes that the
// file stores: the CP932 name and its NUL, followed by what the bytes held
// before, which are random for a card never met. name is the same name in
// UTF-8.
typedef struct {
    SCORE_RECORD_HEADER header;
    int32_t bonus;
    uint16_t index;
    uint8_t checksum;
    uint8_t history[5];
    uint8_t name_bytes[SPELL_CARD_NAME_BYTES];
    char name[SPELL_CARD_NAME_BYTES * 3 + 1];
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

// Holds one entry of a high score list. name holds eight CP932 bytes and a
// NUL. The ASCII font draws text byte by byte, so these bytes are also the
// glyphs that the result screen shows. stage is 99 for a cleared game.
typedef struct {
    // Is HIGH_SCORE_MAGIC for a saved score, or HIGH_SCORE_DEFAULT_MAGIC
    // for the defaults that fill a list and that are never saved.
    uint32_t magic;
    // Marks the entry that the player is naming.
    bool is_new;
    uint8_t reserved[2];
    uint32_t score;
    uint8_t shot;
    uint8_t difficulty;
    uint8_t stage;
    uint8_t name[9];
} HIGH_SCORE;
