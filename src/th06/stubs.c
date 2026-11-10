#include "th06/stubs.h"

#include "core/base/log.h"

RESULT TitleScene_SetUp(const int32_t mode)
{
    LOG_INFO("TitleScene_SetUp is not ported");
    return OK;
}

RESULT GameManager_Register(void)
{
    LOG_INFO("GameManager_Register is not ported");
    return OK;
}

void GameManager_Remove(void)
{
    LOG_INFO("GameManager_Remove is not ported");
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
