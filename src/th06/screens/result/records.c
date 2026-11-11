#include "th06/screens/result/records.h"

#include "th06/game_manager.h"
#include "th06/screens/result/private.h"

#include "core/base/assert.h"
#include "core/base/cp932.h"
#include "core/base/memory.h"
#include "core/base/string.h"

#include <string.h>

#define M_MAX_ATTEMPTS 9999

static SPELL_CARD_RECORD *M_Card(const int32_t index)
{
    ASSERT(index >= 0 && index < SPELL_CARD_COUNT);
    return &g_GameManager.records.spell_cards[index];
}

static void M_StoreName(SPELL_CARD_RECORD *const card, const char *const name)
{
    char *bytes;
    if (!Result_Ignore(Cp932_Encode(name, strlen(name), &bytes))) {
        bytes = Str_Dup("");
    }
    size_t length = strlen(bytes);
    if (length > sizeof(card->name_bytes) - 1) {
        length = sizeof(card->name_bytes) - 1;
    }
    memcpy(card->name_bytes, bytes, length);
    card->name_bytes[length] = 0;
    Mem_Free(bytes);
    char *decoded;
    if (Result_Ignore(
            Cp932_Decode((const char *)card->name_bytes, length, &decoded))) {
        memcpy(card->name, decoded, strlen(decoded) + 1);
        Mem_Free(decoded);
    } else {
        card->name[0] = '\0';
    }
}

void ResultScreen_AddSpellCardAttempt(
    const int32_t index, const char *const name, const int32_t bonus)
{
    SPELL_CARD_RECORD *const card = M_Card(index);
    M_StoreName(card, name);
    uint8_t sum = 0;
    for (size_t i = 0; card->name_bytes[i] != 0; i++) {
        sum = (uint8_t)(sum + card->name_bytes[i]);
    }
    if (card->checksum != sum) {
        card->captures = 0;
        card->attempts = 0;
        card->checksum = sum;
    }
    card->bonus = bonus;
    if (card->attempts < M_MAX_ATTEMPTS) {
        card->attempts++;
    }
}

void ResultScreen_AddSpellCardCapture(
    const int32_t index, const int32_t shot_type)
{
    SPELL_CARD_RECORD *const card = M_Card(index);
    card->captures++;
    for (int32_t i = 4; i > 0; i--) {
        card->history[i] = card->history[i - 1];
    }
    card->history[0] = (uint8_t)shot_type;
}

void ResultScreen_LoadGameRecords(
    uint32_t *const high_score, SPELL_CARD_RECORD spell_cards[SPELL_CARD_COUNT],
    CLEAR_RECORD clears[SCORE_SHOT_COUNT],
    PRACTICE_RECORD practice[SCORE_SHOT_COUNT][SCORE_STAGE_COUNT]
                            [SCORE_PRACTICE_DIFFICULTY_COUNT],
    const int32_t shot, const int32_t difficulty)
{
    const SCORE_FILE *const file = ResultSaves_GetScoreFile();
    *high_score = ScoreFile_GetHighScore(file, shot, difficulty);
    ScoreFile_LoadSpellCards(file, spell_cards);
    ScoreFile_LoadClears(file, clears);
    ScoreFile_LoadPractice(file, practice);
}

void ResultScreen_LoadTitleRecords(
    CLEAR_RECORD clears[SCORE_SHOT_COUNT],
    PRACTICE_RECORD practice[SCORE_SHOT_COUNT][SCORE_STAGE_COUNT]
                            [SCORE_PRACTICE_DIFFICULTY_COUNT])
{
    const SCORE_FILE *const file = ResultSaves_GetScoreFile();
    ScoreFile_LoadClears(file, clears);
    ScoreFile_LoadPractice(file, practice);
}
