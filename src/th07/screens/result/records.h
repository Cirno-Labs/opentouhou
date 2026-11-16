#pragma once

#include <stdint.h>

// Updates, loads and saves the score records that g_GameManager.records
// keeps during a game.

// Starts an attempt at spell card index by g_GameManager.run.shot_index,
// outside replays: stores the CP932 name as the original's strcpy does,
// and resets the counts when the record's checksum does not match. Then
// counts the attempt for the shot and for every shot, up to 9999. A name
// that does not fit the record is cut at 48 bytes, where the original
// overflows.
void ResultScreen_AddSpellCardAttempt(int32_t index, const char *name);

// Counts a capture of spell card index with bonus by
// g_GameManager.run.shot_index, outside replays, for the shot and for every
// shot: the best bonus and the captures, up to 9999.
void ResultScreen_AddSpellCardCapture(int32_t index, int32_t bonus);

// Returns the captures and the attempts of spell card index by
// g_GameManager.run.shot_index.
void ResultScreen_GetSpellCardRecord(
    int32_t index, int32_t *captures, int32_t *attempts);

// Loads the records of score.dat as a new game starts: empties the spell
// cards, then loads the high score of the shot and difficulty and its
// continues, the spell cards that the file has, the clears and the
// practice scores. Keeps a copy of the spell cards in saved_spell_cards.
void ResultScreen_LoadGameRecords(void);

// Loads the clears, the practice scores and the spell cards of score.dat,
// as the title does when it starts.
void ResultScreen_LoadTitleRecords(void);

// Loads the play statistics of score.dat, or empty ones, as the game does
// when it starts.
void ResultScreen_LoadPlayStats(void);

// Saves score.dat as the result screen does when a game starts and when
// the game ends: loads the clears, the practice scores and the spell cards
// from it again, then writes them back with its high scores, its last name
// and the play statistics. Draws the file's key and random byte from the
// gameplay RNG.
void ResultScreen_SaveScore(void);
