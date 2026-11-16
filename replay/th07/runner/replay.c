#include "replay.h"

int32_t RunnerReplay_GetStage(const int32_t slot, const int32_t difficulty)
{
    return slot == 7 && difficulty == 5 ? 8 : slot;
}
