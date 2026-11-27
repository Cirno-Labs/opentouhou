#pragma once

#include "th02/draw.h"
#include "th02/entity.h"

#include "core/vfs.h"

#define POINT_NUMBER_COUNT 20

typedef struct {
    uint8_t rows[13][8];
} POINT_NUMBER_FONT;

typedef struct {
    int16_t left;
    int16_t top[2];
    uint16_t value;
    uint8_t state;
    uint8_t age;
} POINT_NUMBER;

typedef struct {
    POINT_NUMBER slots[POINT_NUMBER_COUNT];
    uint8_t color;
    uint8_t operation;
    uint8_t operand;
} POINT_NUMBERS;

extern const POINT_NUMBERS *const g_PointNumbers;

// Clears the point numbers, once per MAIN process.
void PointNumbers_Init(void);

// Loads the point-number digits.
RESULT PointNumberFont_Load(VFS *files, POINT_NUMBER_FONT *out);

// Frees every slot at stage start, keeping payloads, and sets the rank's
// operation; other ranks keep the previous one.
void PointNumbers_ResetStage(uint8_t rank);

// Adds a point number; a full bank adds nothing.
void PointNumbers_Add(int16_t left, int16_t top, uint16_t value);

// Invalidates the tiles under the point numbers and frees removed slots.
void PointNumbers_Invalidate(PLAYFIELD_PAGES pages);

// Moves and draws the point numbers, values below 13000; as in retail,
// negative X aliases planes.
void PointNumbers_Update(PLAYFIELD_PAGES pages, const POINT_NUMBER_FONT *font);
