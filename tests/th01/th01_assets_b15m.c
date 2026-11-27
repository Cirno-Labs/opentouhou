#include "../../src/th01/boss/b15m.c"
#include "th01_assets.h"

const void *Th01TestAssets_Elis(
    const ALLOCATOR *const a, const TH01_TEST_BOSS_BACKING *const backing)
{
    const GAMEPLAY_ASSETS *const assets = backing->assets;
    uint8_t *const pixels = backing->pixels;
    uint8_t *const coverage = backing->coverage;
    const PI_IMAGE image = backing->image;
    B15M_ASSETS *const out = Allocator_AllocZ(a, sizeof(*out));
    out->girl = (BOS) { 128, 96, 6, pixels, coverage };
    out->attack = (BOS) { 128, 96, 3, pixels, coverage };
    out->bat = (BOS) { 48, 32, 3, pixels, coverage };
    out->spheres = (GRC) { 32, 32, 6, coverage };
    out->star = pixels;
    out->background = image;
    out->missiles = assets->stage;
    out->next_scene = assets->scenes[5];
    return out;
}
