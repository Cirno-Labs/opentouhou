// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "th06/screens/result/types.h"

#include "core/base/result.h"
#include "core/io/reader.h"
#include "core/io/writer.h"

#include <stdint.h>

// Reads and writes score.dat, the original's score file: a 20-byte header,
// then chunks of high scores, clear records, spell-card records and
// practice scores. Every byte from the third on is stored XORed with a key
// that runs over the bytes before it.
//
// Each Load function scans the chunks as the original does, separately
// and in file order, and stops where the original stops: at an index out
// of range for its records. A chunk of size 0, or one that reaches past
// the data, also ends a scan, where the original reads out of bounds or
// never ends.

#define SCORE_LIST_SIZE 10
#define SCORE_DIFFICULTY_COUNT 5
#define SCORE_SHOT_COUNT 4
#define SCORE_STAGE_COUNT 6
#define SCORE_PRACTICE_DIFFICULTY_COUNT 4
#define SCORE_DEFAULT_HIGH_SCORE 1000000
#define HIGH_SCORE_MAGIC 0x52435348 // "HSCR"
#define HIGH_SCORE_DEFAULT_MAGIC 0x53594d44 // "DMYS"

// Holds the decrypted bytes of a score file. A zeroed SCORE_FILE is the
// empty file that the original makes when score.dat is missing or invalid.
typedef struct {
    uint8_t *data;
    int32_t size;
} SCORE_FILE;

// Holds the first ten entries of a high score list, best first. The
// original's lists grow without end, but no entry after the tenth is ever
// shown, saved or moved up.
typedef struct {
    HIGH_SCORE entries[SCORE_LIST_SIZE];
    int32_t count;
} SCORE_LIST;

// Holds what ScoreFile_Write saves besides the high score lists.
typedef struct {
    const SPELL_CARD_RECORD *spell_cards; // SPELL_CARD_COUNT records
    const CLEAR_RECORD *clears; // SCORE_SHOT_COUNT records
    // By shot, stage and difficulty.
    const PRACTICE_RECORD (
        *practice)[SCORE_STAGE_COUNT][SCORE_PRACTICE_DIFFICULTY_COUNT];
} SCORE_RECORDS;

// Reads and decrypts a score file. Reports failure when the file is
// shorter than its header, when its checksum does not match, when its
// offsets reach past the data, or when the original's scan finds no TH6K
// chunk. *out is then empty. ScoreFile_Free releases *out in both cases.
RESULT ScoreFile_Read(READER *reader, SCORE_FILE *out);
// Releases the bytes of file and leaves it empty.
void ScoreFile_Free(SCORE_FILE *file);

// Fills list with the ten defaults of difficulty: "Nanashi ", stage 1,
// from 1,000,000 down to 100,000.
void ScoreList_InitDefaults(SCORE_LIST *list, int32_t difficulty);
// Inserts entry before the first entry whose score is not higher, so that
// a tie places entry first. Returns the rank of entry, from 0. A rank of
// SCORE_LIST_SIZE leaves list unchanged.
int32_t ScoreList_Insert(SCORE_LIST *list, const HIGH_SCORE *entry);

// Inserts the high scores of shot and difficulty from file into list, in
// file order.
void ScoreFile_LoadHighScores(
    const SCORE_FILE *file, SCORE_LIST *list, int32_t shot, int32_t difficulty);
// Returns the best high score of shot and difficulty in file, but at least
// SCORE_DEFAULT_HIGH_SCORE.
uint32_t ScoreFile_GetHighScore(
    const SCORE_FILE *file, int32_t shot, int32_t difficulty);
// Copies the spell-card records of file over cards, by their index, and
// leaves the records that file does not have unchanged.
void ScoreFile_LoadSpellCards(
    const SCORE_FILE *file, SPELL_CARD_RECORD cards[SPELL_CARD_COUNT]);
// Sets every clear record to stage 1, then copies the clear records of
// file over them, by shot.
void ScoreFile_LoadClears(
    const SCORE_FILE *file, CLEAR_RECORD clears[SCORE_SHOT_COUNT]);
// Sets every practice score to 0, then copies the practice records of file
// over them. The original indexes its array by shot * 24 + stage * 4 +
// difficulty and admits difficulty 4 and stage 6, so such a record lands
// in a neighbouring entry. A record whose index is past the array is
// skipped.
void ScoreFile_LoadPractice(
    const SCORE_FILE *file,
    PRACTICE_RECORD practice[SCORE_SHOT_COUNT][SCORE_STAGE_COUNT]
                            [SCORE_PRACTICE_DIFFICULTY_COUNT]);

// Writes an encrypted score file: the header, a TH6K chunk, the saved
// entries among the first ten of each list by difficulty and shot, the
// clear records, the spell-card records with the CATK magic and the
// practice scores that are not 0. Sets the sizes, versions and indices of
// the records as the original does. Keeps the two header bytes that the
// original copies from previous, the score file loaded before. key and
// random are the original's two draws from the gameplay RNG.
void ScoreFile_Write(
    WRITER *writer, const SCORE_FILE *previous,
    const SCORE_LIST lists[SCORE_DIFFICULTY_COUNT][SCORE_SHOT_COUNT],
    const SCORE_RECORDS *records, uint8_t key, uint8_t random);
