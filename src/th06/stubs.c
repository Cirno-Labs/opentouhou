#include "th06/stubs.h"

#include "core/base/log.h"

RESULT TitleScene_SetUp(const int32_t mode)
{
    LOG_INFO("TitleScene_SetUp is not ported");
    return OK;
}

RESULT ResultScreen_Register(const int32_t mode)
{
    LOG_INFO("ResultScreen_Register is not ported");
    return OK;
}

RESULT MusicRoom_Register(void)
{
    LOG_INFO("MusicRoom_Register is not ported");
    return OK;
}

RESULT Ending_Register(void)
{
    LOG_INFO("Ending_Register is not ported");
    return OK;
}

void Replay_Save(void)
{
    LOG_INFO("Replay_Save is not ported");
}

void ResultScreen_AddSpellCardAttempt(
    const int32_t index, const char *const name, const int32_t bonus)
{
    LOG_INFO("ResultScreen_AddSpellCardAttempt is not ported");
}

void ResultScreen_AddSpellCardCapture(
    const int32_t index, const int32_t shot_type)
{
    LOG_INFO("ResultScreen_AddSpellCardCapture is not ported");
}

RESULT Replay_Register(const int32_t mode, const char *const path)
{
    LOG_INFO("Replay_Register is not ported");
    return OK;
}

void Replay_EndStage(void)
{
    LOG_INFO("Replay_EndStage is not ported");
}

void ResultScreen_LoadGameRecords(
    uint32_t *const high_score, SPELL_CARD_RECORD *const spell_cards,
    CLEAR_RECORD *const clears, PRACTICE_RECORD *const practice,
    const int32_t shot, const int32_t difficulty)
{
    LOG_INFO("ResultScreen_LoadGameRecords is not ported");
    *high_score = 1000000;
    for (int32_t i = 0; i < 4; i++) {
        clears[i] = (CLEAR_RECORD) {
            .header = { .magic = 0x44524c43,
                        .size = 0x18,
                        .size2 = 0x18,
                        .version = 0x10 },
            .without_continues = { 1, 1, 1, 1, 1 },
            .with_continues = { 1, 1, 1, 1, 1 },
            .shot = (uint8_t)i,
        };
        for (int32_t j = 0; j < 6; j++) {
            for (int32_t k = 0; k < 4; k++) {
                practice[i * 24 + j * 4 + k] = (PRACTICE_RECORD) {
                    .header = { .magic = 0x52435350,
                                .size = 0x14,
                                .size2 = 0x14,
                                .version = 0x10 },
                    .shot = (uint8_t)i,
                    .difficulty = (uint8_t)k,
                    .stage = (uint8_t)j,
                };
            }
        }
    }
}

void AsciiText_Initialize(void)
{
    LOG_INFO("AsciiText_Initialize is not ported");
}
