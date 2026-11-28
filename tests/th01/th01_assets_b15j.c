#include "tests/th01/th01_assets.h"
#include "th01/boss/private_b15j.h"

const void *Th01TestAssets_Kikuri(
    const ALLOCATOR *const a, const TH01_TEST_BOSS_BACKING *const backing)
{
    const GAMEPLAY_ASSETS *const assets = backing->assets;
    uint8_t *const pixels = backing->pixels;
    uint8_t *const coverage = backing->coverage;
    const PI_IMAGE image = backing->image;
    B15J_ASSETS *const out = Allocator_AllocZ(a, sizeof(*out));
    out->souls = (BOS) { 32, 32, 3, pixels, coverage };
    out->tears = (BOS) { 16, 16, 3, pixels, coverage };
    out->ripples = (PTN) { .count = 1, .pixels = pixels };
    out->background = image;
    out->next_scene = assets->scenes[6];
    return out;
}
