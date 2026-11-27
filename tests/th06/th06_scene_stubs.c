// Stands in for the scene targets that the scene module calls, whose real
// modules link the simulation modules that the tests of those modules fake.
#include "th06/game_manager.h"
#include "th06/replay.h"
#include "th06/screens/result.h"
#include "th06/screens/title.h"

RESULT TitleScene_SetUp(const int32_t mode)
{
    return OK;
}

RESULT GameManager_Register(void)
{
    return OK;
}

void GameManager_Remove(void)
{
}

RESULT ResultScreen_Register(const int32_t mode)
{
    return OK;
}

void Replay_Discard(void)
{
}
