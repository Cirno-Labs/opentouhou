#pragma once

#include "th07/screens/result/types.h"

#include "core/base/result.h"
#include "core/io/reader.h"
#include "core/io/writer.h"

#include <stdint.h>

// Reads and writes score.dat, the original's score file: a 28-byte header,
// then chunks packed with LZSS: TH7K, high scores, clear records,
// spell-card records, practice scores, the last name, the play statistics
// and the game version. Every byte from the third on is stored XORed with
// a key that runs over the bytes before it.
//
// Each Load function scans the chunks as the original does, separately
// and in file order, and stops where the original stops: at an index out
// of range for its records. A record that reaches past the data also ends
// a scan, where the original reads out of bounds.

#define SCORE_LIST_SIZE 10
#define SCORE_DEFAULT_HIGH_SCORE 100000
#define HIGH_SCORE_MAGIC 0x52435348 // "HSCR"
#define HIGH_SCORE_DEFAULT_MAGIC 0x53594d44 // "DMYS"
#define SPELL_CARD_MAGIC 0x4b544143 // "CATK"

// Holds the header and the unpacked chunks of a score file. A zeroed
// SCORE_FILE is the empty file that the original makes when score.dat is
// missing or invalid.
typedef struct {
    uint8_t *data;
    int32_t size;
} SCORE_FILE;

// Holds the first ten entries of a high score list, best first. The
// original's lists grow without end, but no entry after the tenth is ever
// shown or saved.
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
    const LAST_NAME_RECORD *last_name;
    const PLAY_STATS_RECORD *play_stats;
} SCORE_RECORDS;

// Reads, decrypts and unpacks a score file. Reports failure when the file
// is shorter than its header, when its checksum, header size or version
// does not match, when it does not unpack, when a chunk header reaches
// past the data or has size 0, or when no TH7K chunk of version 1 is
// found. *out is then empty. ScoreFile_Free releases *out in both cases.
// The game version chunks are kept without the check of the executable
// that the original makes.
RESULT ScoreFile_Read(READER *reader, SCORE_FILE *out);

// Releases the bytes of file and leaves it empty.
void ScoreFile_Free(SCORE_FILE *file);

// Fills list with the ten defaults of difficulty: "--------", stage 1,
// from 100,000 down to 10,000.
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
// SCORE_DEFAULT_HIGH_SCORE, and sets *continues to the continues of the
// best entry, or to 0 without one.
uint32_t ScoreFile_GetHighScore(
    const SCORE_FILE *file, int32_t shot, int32_t difficulty,
    uint8_t *continues);

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

// Copies the first last-name record of file to name. Returns false, and
// leaves name unchanged, when file has none.
bool ScoreFile_LoadLastName(const SCORE_FILE *file, LAST_NAME_RECORD *name);

// Copies the last play-statistics record of file to stats, or leaves stats
// unchanged when file has none.
void ScoreFile_LoadPlayStats(const SCORE_FILE *file, PLAY_STATS_RECORD *stats);

// Writes a packed and encrypted score file: the header, a TH7K chunk, the
// saved entries among the first ten of each list by difficulty and shot,
// the clear records, the spell-card records with the CATK magic, the
// practice scores that are not 0, the last name, the play statistics and
// a game version chunk. Sets the sizes, versions and indices of the
// records as the original does. Keeps the header bytes that the original
// copies from previous, the score file loaded before. key and random are
// the original's two draws from the gameplay RNG. The game version chunk
// holds 0 as the size and checksum of the executable.
void ScoreFile_Write(
    WRITER *writer, const SCORE_FILE *previous,
    const SCORE_LIST lists[SCORE_DIFFICULTY_COUNT][SCORE_SHOT_COUNT],
    const SCORE_RECORDS *records, uint8_t key, uint8_t random);
