#include "tests/th01/th01_assets.h"
#include "th01/boss/b05.c"

const void *Th01TestAssets_Singyoku(
    const ALLOCATOR *const a, const TH01_TEST_BOSS_BACKING *const backing)
{
    uint8_t *const pixels = backing->pixels;
    uint8_t *const coverage = backing->coverage;
    const PI_IMAGE image = backing->image;
    B05_ASSETS *const out = Allocator_AllocZ(a, sizeof(*out));
    out->sphere = (BOS) { 96, 96, 8, pixels, coverage };
    out->flash = (BOS) { 96, 96, 3, pixels, coverage };
    out->person = (BOS) { 96, 96, 5, pixels, coverage };
    out->background = image;
    return out;
}
