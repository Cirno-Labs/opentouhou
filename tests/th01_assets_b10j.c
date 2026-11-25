#include "../src/th01/boss_b10j.c"
#include "th01_assets.h"

const void *Th01TestAssets_Mima(
    const ALLOCATOR *const a, const TH01_TEST_BOSS_BACKING *const backing)
{
    const GAMEPLAY_ASSETS *const assets = backing->assets;
    uint8_t *const pixels = backing->pixels;
    uint8_t *const coverage = backing->coverage;
    const PI_IMAGE image = backing->image;
    B10J_ASSETS *const out = Allocator_AllocZ(a, sizeof(*out));
    out->still = (BOS) { 128, 160, 1, pixels, coverage };
    out->animation = (BOS) { 128, 64, 5, pixels, coverage };
    out->background = image;
    out->missiles = assets->stage;
    out->next_scene = assets->scenes[4];
    return out;
}
