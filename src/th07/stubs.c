#include "th07/stubs.h"

#include "th07/game_manager.h"

#include "core/base/log.h"

RESULT ResultScreen_Register(const int32_t mode)
{
    LOG_INFO("ResultScreen_Register is not ported");
    return OK;
}

RESULT MusicScreen_Register(void)
{
    LOG_INFO("MusicScreen_Register is not ported");
    return OK;
}

RESULT Ending_Register(void)
{
    LOG_INFO("Ending_Register is not ported");
    return OK;
}

void ResultScreen_AddSpellCardAttempt(
    const int32_t index, const char *const name)
{
    LOG_INFO("ResultScreen_AddSpellCardAttempt is not ported");
}

void ResultScreen_AddSpellCardCapture(const int32_t index, const int32_t bonus)
{
    int32_t *const captures = &g_GameManager.records.spell_captures[index];
    if (*captures < 9999) {
        (*captures)++;
    }
}

void ResultScreen_GetSpellCardRecord(
    const int32_t index, int32_t *const captures, int32_t *const attempts)
{
    *captures = 0;
    *attempts = 0;
}

void Replay_ResaveIfScoreChanged(void)
{
    LOG_INFO("Replay_ResaveIfScoreChanged is not ported");
}

RESULT ResultScreen_LoadGameRecords(void)
{
    LOG_INFO("ResultScreen_LoadGameRecords is not ported");
    return OK;
}

uint8_t g_EffectsQuality = 2;

void ResultScreen_LoadTitleRecords(void)
{
    LOG_INFO("ResultScreen_LoadTitleRecords is not ported");
}

void PlayTime_Add(void)
{
    LOG_INFO("PlayTime_Add is not ported");
}

void PlayTime_Restart(void)
{
    LOG_INFO("PlayTime_Restart is not ported");
}

void PlayTime_Stop(void)
{
    LOG_INFO("PlayTime_Stop is not ported");
}
