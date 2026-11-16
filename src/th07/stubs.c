#include "th07/stubs.h"

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

void Replay_ResaveIfScoreChanged(void)
{
    LOG_INFO("Replay_ResaveIfScoreChanged is not ported");
}

uint8_t g_EffectsQuality = 2;

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
