#include "gameplay_sample.h"

#include "th02/background.h"
#include "th02/player.h"
#include "th02/random.h"
#include "th02/score.h"
#include "th02/stage.h"

void Th02_SampleGameplay(TH02_GAMEPLAY_SAMPLE *const out)
{
    *out = (TH02_GAMEPLAY_SAMPLE) {
        .score = g_Score->value,
        .lives = g_Score->lives,
        .bombs = g_Score->bombs,
        .power = g_Player->power,
        .rng = g_Rng->seed,
        .rng_cursor = g_Rng->cursor,
        .player_x = g_Player->current.x,
        .player_y = g_Player->current.y,
    };
}

uint32_t Th02_LoadedTileSources(void)
{
    return g_Background->last_source + 1u;
}
