#include "../../src/th01/boss/b20m.c"
#include "th01_assets.h"

const void *Th01TestAssets_Sariel(
    const ALLOCATOR *const a, const TH01_TEST_BOSS_BACKING *const backing)
{
    uint8_t *const pixels = backing->pixels;
    uint8_t *const coverage = backing->coverage;
    const PI_IMAGE image = backing->image;
    B20M_ASSETS *const out = Allocator_AllocZ(a, sizeof(*out));
    out->shield = (BOS) { 48, 48, 4, pixels, coverage };
    out->wand = (BOS) { 96, 96, 2, pixels, coverage };
    out->dress = (BOS) { 96, 96, 4, pixels, coverage };
    out->vortex = (GRC) { 32, 32, 6, coverage };
    out->bird = (GRC) { 32, 32, 8, coverage };
    out->cross = (GRC) { 32, 32, 2, coverage };
    out->splash = (GRC) { 32, 32, 3, coverage };
    out->lower = image;
    out->upper = image;
    for (int32_t i = 0; i < 6; i++) {
        out->backgrounds[i] = image;
    }
    out->star = pixels;
    out->flake = pixels;
    out->spark = pixels;
    out->leaf_left = pixels;
    out->leaf_right = pixels;
    out->interleave = Allocator_AllocZ(a, sizeof(INTERLEAVE_ASSETS));
    return out;
}
