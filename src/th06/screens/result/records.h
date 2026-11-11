#pragma once

#include "th06/screens/result/score.h"

#include <stdint.h>

// Updates and loads the score records that g_GameManager keeps during a
// game. The result screen saves them to score.dat.

// Starts an attempt at spell card index, outside replays: stores the CP932
// name as the original's strcpy does, and resets the attempts and captures
// when the byte sum of the name differs from the stored one. Then stores
// bonus and counts the attempt, up to 9999. A name that does not fit the
// record is cut at 35 bytes, where the original overflows.
void ResultScreen_AddSpellCardAttempt(
    int32_t index, const char *name, int32_t bonus);
// Counts a capture of spell card index by shot_type, outside replays, and
// puts shot_type first in its history.
void ResultScreen_AddSpellCardCapture(int32_t index, int32_t shot_type);
// Loads the records of score.dat at the start of a game: the high score of
// shot and difficulty, the spell cards that the file has, the clears and
// the practice scores.
void ResultScreen_LoadGameRecords(
    uint32_t *high_score, SPELL_CARD_RECORD spell_cards[SPELL_CARD_COUNT],
    CLEAR_RECORD clears[SCORE_SHOT_COUNT],
    PRACTICE_RECORD practice[SCORE_SHOT_COUNT][SCORE_STAGE_COUNT]
                            [SCORE_PRACTICE_DIFFICULTY_COUNT],
    int32_t shot, int32_t difficulty);
