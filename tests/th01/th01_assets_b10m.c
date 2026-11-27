#include "tests/th01/th01_assets.h"
#include "th01/boss/b10m.c"

const void *Th01TestAssets_YuugenMagan(
    const ALLOCATOR *const a, const TH01_TEST_BOSS_BACKING *const backing)
{
    const GAMEPLAY_ASSETS *const assets = backing->assets;
    uint8_t *const pixels = backing->pixels;
    uint8_t *const coverage = backing->coverage;
    const PI_IMAGE image = backing->image;
    B10M_ASSETS *const out = Allocator_AllocZ(a, sizeof(*out));
    out->eyes = (BOS) { 64, 48, 7, pixels, coverage };
    out->background = image;
    out->missiles = assets->stage;
    out->next_scene = assets->scenes[3];
    return out;
}
