#include "th07/screens/result/records.h"

#include "th07/game.h"
#include "th07/game_manager.h"
#include "th07/screens/result/private.h"

#include "core/base/assert.h"
#include "core/base/cp932.h"
#include "core/base/memory.h"
#include "core/base/string.h"

#include <string.h>

#define M_MAX_COUNT 9999
#define M_SPELL_CARD_SIZE 0x78
#define M_LAST_NAME_MAGIC 0x4d4e534c
#define M_PLAY_STATS_MAGIC 0x54534c50
#define M_PLAY_STATS_SIZE 0x160

// Holds the high score lists that ResultScreen_SaveScore writes.
static SCORE_LIST m_Lists[SCORE_DIFFICULTY_COUNT][SCORE_SHOT_COUNT];

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
    if (length > sizeof(card->name) - 1) {
        length = sizeof(card->name) - 1;
    }
    memcpy(card->name, bytes, length);
    card->name[length] = 0;
    Mem_Free(bytes);
}

// Returns the sum of the name bytes up to the NUL.
static uint32_t M_NameSum(const SPELL_CARD_RECORD *const card)
{
    uint32_t sum = 0;
    for (size_t i = 0; i < sizeof(card->name) && card->name[i] != 0; i++) {
        sum += card->name[i];
    }
    return sum;
}

static uint32_t M_CountSum(const SPELL_CARD_RECORD *const card)
{
    uint32_t sum = 0;
    for (int32_t i = 0; i < 7; i++) {
        sum += card->captures[i] + card->attempts[i] + card->max_bonus[i];
    }
    return sum;
}

// Clears the counts when the checksum does not match the name and counts.
static void M_Check(SPELL_CARD_RECORD *const card)
{
    if (card->checksum != (uint8_t)(M_NameSum(card) + M_CountSum(card))) {
        memset(card->captures, 0, sizeof(card->captures));
        memset(card->attempts, 0, sizeof(card->attempts));
        memset(card->max_bonus, 0, sizeof(card->max_bonus));
    }
}

static void M_Seal(SPELL_CARD_RECORD *const card)
{
    card->checksum = (uint8_t)(M_NameSum(card) + M_CountSum(card));
}

static void M_CountUp(uint16_t *const count)
{
    if (*count < M_MAX_COUNT) {
        (*count)++;
    }
}

void ResultScreen_AddSpellCardAttempt(
    const int32_t index, const char *const name)
{
    SPELL_CARD_RECORD *const card = M_Card(index);
    M_StoreName(card, name);
    M_Check(card);
    M_CountUp(&card->attempts[g_GameManager.run.shot_index]);
    M_CountUp(&card->attempts[SCORE_ALL_SHOTS]);
    M_Seal(card);
}

void ResultScreen_AddSpellCardCapture(const int32_t index, const int32_t bonus)
{
    SPELL_CARD_RECORD *const card = M_Card(index);
    M_Check(card);
    const int32_t shot = g_GameManager.run.shot_index;
    const int32_t slots[] = { shot, SCORE_ALL_SHOTS };
    for (int32_t i = 0; i < 2; i++) {
        // The original compares the bonus as unsigned.
        if (card->max_bonus[slots[i]] < (uint32_t)bonus) {
            card->max_bonus[slots[i]] = (uint32_t)bonus;
        }
    }
    M_CountUp(&card->captures[shot]);
    M_CountUp(&card->captures[SCORE_ALL_SHOTS]);
    M_Seal(card);
}

void ResultScreen_GetSpellCardRecord(
    const int32_t index, int32_t *const captures, int32_t *const attempts)
{
    const SPELL_CARD_RECORD *const card = M_Card(index);
    *captures = card->captures[g_GameManager.run.shot_index];
    *attempts = card->attempts[g_GameManager.run.shot_index];
}

void ResultScreen_LoadGameRecords(void)
{
    GAME_MANAGER *const gm = &g_GameManager;
    memset(gm->records.spell_cards, 0, sizeof(gm->records.spell_cards));
    for (int32_t i = 0; i < SPELL_CARD_COUNT; i++) {
        SPELL_CARD_RECORD *const card = &gm->records.spell_cards[i];
        card->header.magic = SPELL_CARD_MAGIC;
        card->header.size = M_SPELL_CARD_SIZE;
        card->header.size2 = M_SPELL_CARD_SIZE;
        card->header.version = 1;
        card->index = (uint16_t)i;
    }
    const SCORE_FILE *const file = ResultSaves_GetScoreFile();
    gm->score.high = ScoreFile_GetHighScore(
        file, gm->run.shot_index, gm->run.difficulty,
        &gm->score.high_continues);
    ScoreFile_LoadSpellCards(file, gm->records.spell_cards);
    ScoreFile_LoadClears(file, gm->records.clears);
    ScoreFile_LoadPractice(file, gm->records.practice);
    memcpy(
        gm->records.saved_spell_cards, gm->records.spell_cards,
        sizeof(gm->records.saved_spell_cards));
}

void ResultScreen_LoadTitleRecords(void)
{
    const SCORE_FILE *const file = ResultSaves_GetScoreFile();
    ScoreFile_LoadClears(file, g_GameManager.records.clears);
    ScoreFile_LoadPractice(file, g_GameManager.records.practice);
    ScoreFile_LoadSpellCards(file, g_GameManager.records.spell_cards);
}

void ResultScreen_LoadPlayStats(void)
{
    PLAY_STATS_RECORD *const stats = &g_GameManager.records.play_stats;
    memset(stats, 0, sizeof(*stats));
    stats->header.magic = M_PLAY_STATS_MAGIC;
    stats->header.size = M_PLAY_STATS_SIZE;
    stats->header.size2 = stats->header.size;
    stats->header.version = 1;
    ScoreFile_LoadPlayStats(ResultSaves_GetScoreFile(), stats);
}

void ResultScreen_SaveScore(void)
{
    GAME_MANAGER *const gm = &g_GameManager;
    const SCORE_FILE *const file = ResultSaves_GetScoreFile();
    for (int32_t difficulty = 0; difficulty < SCORE_DIFFICULTY_COUNT;
         difficulty++) {
        for (int32_t shot = 0; shot < SCORE_SHOT_COUNT; shot++) {
            SCORE_LIST *const list = &m_Lists[difficulty][shot];
            ScoreList_InitDefaults(list, difficulty);
            ScoreFile_LoadHighScores(file, list, shot, difficulty);
        }
    }
    LAST_NAME_RECORD last_name = {
        .header = { .magic = M_LAST_NAME_MAGIC,
                    .size = 0x18,
                    .size2 = 0x18,
                    .version = 1 },
    };
    memcpy(last_name.name, "        ", 9);
    ScoreFile_LoadLastName(file, &last_name);
    ScoreFile_LoadSpellCards(file, gm->records.spell_cards);
    ScoreFile_LoadClears(file, gm->records.clears);
    GameManager_IsPhantasmOpen();
    ScoreFile_LoadPractice(file, gm->records.practice);

    const SCORE_RECORDS records = {
        .spell_cards = gm->records.spell_cards,
        .clears = gm->records.clears,
        .practice = gm->records.practice,
        .last_name = &last_name,
        .play_stats = &gm->records.play_stats,
    };
    const uint8_t key = (uint8_t)Rng_U16InRange(&g_Rng, 256);
    const uint8_t random = (uint8_t)Rng_U16InRange(&g_Rng, 256);
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator, nullptr);
    ScoreFile_Write(&writer, file, m_Lists, &records, key, random);
    size_t size;
    uint8_t *const data = Writer_TakeBuffer(&writer, &size);
    Result_Ignore(Writer_Close(&writer));
    ResultSaves_PutScoreFile(data, size);
}
