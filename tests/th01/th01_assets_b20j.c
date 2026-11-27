#include "../../src/th01/boss/b20j.c"
#include "th01_assets.h"

const void *Th01TestAssets_Konngara(
    const ALLOCATOR *const a, const TH01_TEST_BOSS_BACKING *const backing)
{
    uint8_t *const pixels = backing->pixels;
    uint8_t *const coverage = backing->coverage;
    const PI_IMAGE image = backing->image;
    B20J_ASSETS *const out = Allocator_AllocZ(a, sizeof(*out));
    out->head = (BOS) { 80, 80, 3, pixels, coverage };
    out->face = (BOS) { 80, 32, 6, pixels, coverage };
    out->aim = (BOS) { 80, 32, 3, pixels, coverage };
    out->body.count = 16;
    for (int32_t i = 0; i < 16; i++) {
        out->body.images[i] =
            (GRX) { .stream_count = 1, .pixels = pixels, .coverage = coverage };
    }
    out->scroll = image;
    out->upper = image;
    for (int32_t i = 0; i < 4; i++) {
        out->layers[i] = image;
    }
    out->diamond = pixels;
    out->flake = pixels;
    return out;
}
