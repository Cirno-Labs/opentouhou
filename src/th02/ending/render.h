#pragma once

#include "th02/ending/assets.h"
#include "th02/ending/ending.h"
#include "th02/input.h"
#include "th02/palette.h"
#include "th02/session.h"

typedef struct {
    int16_t radii[5];
    uint8_t angles[5];
    uint8_t speed;
    uint8_t step;
} ENDING_ROTATION;

typedef enum {
    ENDING_CREDIT_PROGRAM,
    ENDING_CREDIT_GRAPHICS,
    ENDING_CREDIT_MUSIC,
    ENDING_CREDIT_TESTERS,
} ENDING_CREDIT;

typedef struct {
    const ENDING_REVEAL_MASKS *masks;
    int16_t left;
    int16_t top;
    uint16_t columns;
    uint8_t step;
} ENDING_REVEAL;

// Starts a rotation at a byte speed in either direction, drawing nothing.
void EndingRender_BeginRotation(
    ENDING_ROTATION *rotation, uint8_t speed, uint8_t angle);

// Runs the rotation over 20 frames; returns true when done. White RMW
// charging stays on until then.
bool EndingRender_StepRotation(ENDING_ROTATION *rotation);

// Draws image on page one, first showing its palette when asked.
void EndingRender_Stage(
    const ENDING_IMAGES *images, ENDING_IMAGE image, bool apply_palette);

// Copies rows of a picture quarter to an on-screen X divisible by 16, without
// clipping; source_top + rows <= 200.
void EndingRender_Copy(
    int16_t left, int16_t top, uint8_t quarter, int16_t source_top,
    int16_t rows);

// Draws a credits screen; as in retail, the last two lines fall below the
// cleared rectangle.
void EndingRender_Credits(const ENDING_LABELS *labels, ENDING_CREDIT section);

// Draws one verdict row; requires normal rank, up to three continues,
// starting lives 0..4 and bombs 0..5. Skill 77 has its own title.
void EndingRender_VerdictRow(
    const ENDING_LABELS *labels, const ENDING_TEXT_SCRIPT *titles,
    const GAIJI *gaiji, const TH02_SESSION *session, uint8_t row,
    uint16_t style);

// Loads the three reveal masks, bit 15 leftmost.
RESULT EndingRender_LoadRevealMasks(VFS *files, ENDING_REVEAL_MASKS *out);

// Starts revealing an on-screen row at X divisible by eight; even an empty
// row takes three ten-frame waits.
void EndingRender_BeginReveal(
    ENDING_REVEAL *reveal, const ENDING_REVEAL_MASKS *masks, int16_t left,
    int16_t top, uint16_t columns);

// Reveals the next part of the row from page one, without clipping;
// returns true when done.
bool EndingRender_StepReveal(ENDING_REVEAL *reveal);
